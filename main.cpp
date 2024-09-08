
#include <filesystem>
#include <span>
#include <vector>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <vpad/input.h>
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
    VPADInit();
    auto sdPath = std::filesystem::path(WHBGetSdCardMountPath()) / "videos" / "videoplayback.mp4";

    auto byteStreamCapacity = 23'000'000u;

    H264TrackData trackData{};
    trackData.stream.reserve(byteStreamCapacity);
    if (!LoadAVCTrackFromMP4(sdPath, trackData))
    {
        WHBLogPrint("Failed to load track");
        return -1;
    }
    WHBLogPrintf("Loaded track with dim %d x %d", trackData.width, trackData.height);

    const auto decStartOffset = H264Decoder::GetStartPoint(trackData.stream);
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
    gfx->SetFrameDimensions(trackData.width, trackData.height);
    gfx->SetVideoDrawTargets(Gfx::DrawTargets::TV);

    std::optional<H264Decoder> decoder;
    try
    {
        decoder.emplace(static_cast<H264Profile>(trackData.profile), trackData.level, trackData.width, trackData.height);
    }
    catch (const std::exception& e)
    {
        WHBLogPrint(e.what());
        return -1;
    }

    decoder->SubmitFrame(trackData.stream, 0);

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
