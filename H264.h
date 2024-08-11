#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

#include <coreinit/fastmutex.h>
#include <coreinit/messagequeue.h>
#include <h264/decode.h>


class H264DecoderException : public std::exception
{
  public:
    explicit H264DecoderException(std::string_view str);

    explicit H264DecoderException(std::string_view str, H264Error error);

    [[nodiscard]] const char* what() const noexcept override;

  private:
    std::string m_content;
};

enum H264Profile
{
    H264_PROFILE_BASE = 66,
    H264_PROFILE_MAIN = 77,
    H264_PROFILE_HIGH = 100
};

class H264Decoder
{
    using CtxPointer = std::unique_ptr<void, decltype(&std::free)>;
    using FrameBufPointer = std::unique_ptr<uint8_t, decltype(&std::free)>;
    struct InputFrameInfo
    {
        std::span<const uint8_t> buffer;
        double timestamp;
    };

  public:
    struct OutputFrameInfo
    {
        std::vector<uint8_t> buffer;
        int32_t width;
        int32_t height;
        int32_t pitch;
        double timestamp;
    };

  public:
    static int32_t GetStartPoint(std::span<const uint8_t> buffer);

    explicit H264Decoder(H264Profile profile, unsigned level, unsigned width, unsigned height);
    ~H264Decoder();
    void SubmitFrame(std::span<const uint8_t> data, double timestamp);
    std::optional<OutputFrameInfo> GetDecodedFrame();

  private:
    static void DecodeCallback(H264DecodeOutput* output);
    void DecoderLoop();

  private:
    FrameBufPointer m_frameBuffer;
    CtxPointer m_context;
    std::vector<OSMessage> m_messageBuffer;

    std::deque<InputFrameInfo> m_framesIn{};
    OSFastMutex m_mutexIn{};

    OSMessageQueue m_frameOutQueue;

    std::jthread m_thread;
    std::atomic_bool m_running;
};
