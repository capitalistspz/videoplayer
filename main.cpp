#include <whb/sdcard.h>
#include <whb/file.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <cstring>
#include <filesystem>
#include <span>
#include <coreinit/thread.h>
#include <vector>

#include "H264.h"
#include "MP4.h"

int main() {
    WHBLogCafeInit();
    WHBMountSdCard();
    auto sdPath = std::filesystem::path(WHBGetSdCardMountPath()) / "videos" / "videoplayback.mp4";

    auto byteStreamCapacity = 23'000'000u;

    std::vector<uint8_t> byteStream;
    byteStream.reserve(byteStreamCapacity);
    if (!LoadAVCTrackFromMP4(sdPath, byteStream)) {
        WHBLogPrint("Failed to load track");
        return -1;
    }

    const auto decStartOffset  = H264Decoder::GetStartPoint(byteStream);
    if (decStartOffset < 0) {
        WHBLogPrint("Failed to find start");
    } else {
        WHBLogPrintf("Found start at %d", decStartOffset);
    }

    auto decoder = H264Decoder(H264_PROFILE_MAIN, 30, 640, 360);

    decoder.SubmitFrame(byteStream, 0);

    WHBLogPrint("Waiting for frame");

    auto frameInfo = decoder.GetDecodedFrame();
    while (!frameInfo) {
        frameInfo = decoder.GetDecodedFrame();
    }

    WHBLogPrint("Got decoded frame");

    auto outImage = std::filesystem::path(WHBGetSdCardMountPath()) / "videos" / "decoded.nv12";

    WHBLogPrintf("Frame buffer size: %d", frameInfo->buffer.size());

    auto *file = std::fopen(outImage.c_str(), "wb");
    WHBLogPrint("Opened file");
    for (auto lineNo = 0; lineNo < frameInfo->height * 3 / 2; ++lineNo) {
        std::fwrite(frameInfo->buffer.data() + lineNo * frameInfo->pitch, 1, frameInfo->width, file);
    }
    WHBLogPrint("Finished writing to file");
    std::fclose(file);

    WHBLogCafeDeinit();
    return 0;
}
