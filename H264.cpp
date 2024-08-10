#include "H264.h"

#include <whb/log.h>
#include <format>
#include <mutex>
#include <utility>
#include <coreinit/spinlock.h>

struct ScopedLock {
    explicit ScopedLock(OSFastMutex &mutex) noexcept
        : m_lock(&mutex) {
        OSFastMutex_Lock(m_lock);
    }

    ~ScopedLock() noexcept {
        OSFastMutex_Unlock(m_lock);
    }

    OSFastMutex *m_lock;
};


void *H264Alloc(uint32_t size) {
    while (true) {
        auto *ptr = std::aligned_alloc(0x100, size);
        if (H264DECCheckMemSegmentation(ptr, size) == H264_ERROR_OK)
            return ptr;
        free(ptr);
    }
}

H264DecoderException::H264DecoderException(std::string_view str): m_content(str) {
}

H264DecoderException::H264DecoderException(std::string_view str, H264Error error)
    : m_content(
        std::format("{}: {} ({:#x})", str, std::to_underlying(error), std::to_underlying(error))
    ) {
}

const char *H264DecoderException::what() const noexcept {
    return m_content.data();
}

int32_t H264Decoder::GetStartPoint(std::span<const uint8_t> buffer) {
    int32_t decStartOffset = 0;

    if (H264DECFindDecstartpoint(buffer.data(), buffer.size(), &decStartOffset) != 0)
        return -1;
    return decStartOffset;
}

H264Decoder::H264Decoder(H264Profile profile, unsigned level, unsigned width, unsigned height)
    : m_frameBuffer(static_cast<uint8_t *>(H264Alloc(width * height * 3)), std::free), m_context(
          nullptr, nullptr), m_messageBuffer(80) {
    uint32_t h264MemReq;
    auto h264Error = H264DECMemoryRequirement(profile, level, width, height, &h264MemReq);
    if (h264Error) {
        throw H264DecoderException("Failed to get memory requirement", h264Error);
    }
    m_context = CtxPointer(H264Alloc(h264MemReq), std::free);

    h264Error = H264DECInitParam(h264MemReq, m_context.get());
    if (h264Error) {
        throw H264DecoderException("Failed to init decoder: ", h264Error);
    }
    h264Error = H264DECSetParam_FPTR_OUTPUT(m_context.get(), H264Decoder::DecodeCallback);
    if (h264Error) {
        throw H264DecoderException("Failed to set decode callback: ", h264Error);
    }
    auto temp = this;
    // The user memory value is dereferenced when set
    h264Error = H264DECSetParam_USER_MEMORY(m_context.get(), &temp);
    if (h264Error) {
        throw H264DecoderException("Failed to set decoder arg: ", h264Error);
    }
    WHBLogPrintf("Set user memory to %p", this);
    h264Error = H264DECOpen(m_context.get());
    if (h264Error) {
        throw H264DecoderException("Failed to open session: ", h264Error);
    }
    m_running = true;
    OSFastMutex_Init(&m_mutexIn, "decoderInputMutex");

    auto messageCount = m_messageBuffer.size();
    WHBLogPrintf("Message count: %u", messageCount);
    OSInitMessageQueueEx(&m_frameOutQueue, m_messageBuffer.data(), messageCount, "decoderOutputQueue");
    m_thread = std::jthread([this] { this->DecoderLoop(); });
}

H264Decoder::~H264Decoder() {
    m_running = false;
}

void H264Decoder::SubmitFrame(std::span<const uint8_t> data, double timestamp) {
    ScopedLock lock(m_mutexIn);
    m_framesIn.emplace_back(data, timestamp);
}

std::optional<H264Decoder::OutputFrameInfo> H264Decoder::GetDecodedFrame() {
    OSMessage message;
    if (!OSReceiveMessage(&m_frameOutQueue, &message, OS_MESSAGE_FLAGS_NONE)) {
        return std::nullopt;
    }
    auto *out = static_cast<OutputFrameInfo *>(message.message);
    auto ret = std::make_optional(std::move(*out));
    delete out;
    return ret;
}

void H264Decoder::DecoderLoop() {
    while (m_running) {
        ScopedLock lock(m_mutexIn);
        if (m_framesIn.empty())
            continue;
        auto frame = m_framesIn.front();
        H264DECBegin(m_context.get());
        H264DECSetBitstream(m_context.get(), const_cast<uint8_t *>(frame.buffer.data()), frame.buffer.size(),
                            frame.timestamp);
        H264DECExecute(m_context.get(), m_frameBuffer.get());
        H264DECEnd(m_context.get());
        m_framesIn.pop_front();
    }
    H264DECClose(m_context.get());
}

void H264Decoder::DecodeCallback(H264DecodeOutput *output) {
    if (output->frameCount < 1)
        return;
    auto *origin = static_cast<H264Decoder *>(output->userMemory);
    WHBLogPrintf("User memory is %p", origin);

    WHBLogPrint("Locked");
    for (auto i = 0; i < output->frameCount; ++i) {
        const auto &current = output->decodeResults[i];
        const unsigned frameByteCount = current->height * current->nextLine * 3 / 2;
        auto frameSpan = std::span(static_cast<uint8_t *>(current->framebuffer), frameByteCount);

        auto *frameInfo = new OutputFrameInfo{
            {frameSpan.begin(), frameSpan.end()},
            current->width,
            current->height,
            current->nextLine,
            current->timestamp
        };

        WHBLogPrint("Enqueuing frame");
        OSMessage msg{frameInfo, {}};
        OSSendMessage(&origin->m_frameOutQueue, &msg, OS_MESSAGE_FLAGS_NONE);
        WHBLogPrint("Enqueued frame");
    }
}
