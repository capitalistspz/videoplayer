
#include <filesystem>
#include <span>
#include <vector>

#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#include <nv12torgb_frag.h>
#include <nv12torgb_vert.h>
#include <CafeGLSLCompiler.h>

#include "Gfx.h"
#include "H264.h"
#include "MP4.h"

int main()
{
    WHBProcInit();
    WHBGfxInit();
    GLSL_Init();
    WHBLogCafeInit();
    WHBMountSdCard();
    auto sdPath = std::filesystem::path(WHBGetSdCardMountPath()) / "videos" / "videoplayback.mp4";

    auto byteStreamCapacity = 23'000'000u;

    std::vector<uint8_t> byteStream;
    byteStream.reserve(byteStreamCapacity);
    unsigned trackWidth, trackHeight;
    if (!LoadAVCTrackFromMP4(sdPath, byteStream, trackWidth, trackHeight))
    {
        WHBLogPrint("Failed to load track");
        return -1;
    }
    WHBLogPrintf("Loaded track with dim %d x %d", trackWidth, trackHeight);

    const auto decStartOffset = H264Decoder::GetStartPoint(byteStream);
    if (decStartOffset < 0)
    {
        WHBLogPrint("Failed to find start");
    }
    else
    {
        WHBLogPrintf("Found start at %d", decStartOffset);
    }

    const auto vtxShaderSrc = std::string(reinterpret_cast<const char*>(nv12torgb_vert), nv12torgb_vert_size);
    const auto pixShaderSrc = std::string(reinterpret_cast<const char*>(nv12torgb_frag), nv12torgb_frag_size);

    std::unique_ptr<Gfx> gfx;

    try
    {
        gfx = std::make_unique<Gfx>(vtxShaderSrc, pixShaderSrc);
    }
    catch (const std::exception& e)
    {
        WHBLogPrint(e.what());
        return -1;
    }
    gfx->SetFrameDimensions(trackWidth, trackHeight);
    gfx->SetVideoDrawTargets(Gfx::DrawTargets::TV);

    std::optional<H264Decoder> decoder;
    try
    {
        decoder.emplace(H264_PROFILE_MAIN, 30, trackWidth, trackHeight);
    }
    catch (const std::exception& e)
    {
        WHBLogPrint(e.what());
        return -1;
    }

    decoder->SubmitFrame(byteStream, 0);

    auto frameInfo = decoder->GetDecodedFrame();
    while (!frameInfo)
    {
        frameInfo = decoder->GetDecodedFrame();
    }

    gfx->SetFrameBuffer(*frameInfo);
    while (WHBProcIsRunning())
    {
        gfx->Draw();
    }

    GLSL_Shutdown();
    WHBGfxShutdown();
    WHBProcShutdown();
    WHBLogCafeDeinit();
    return 0;
}
