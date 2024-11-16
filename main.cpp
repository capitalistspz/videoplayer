#include "Gfx.h"
#include "H264.h"
#include "MP4.h"
#include <filesystem>
#include <span>
#include <sysapp/launch.h>
#include <vector>
#include <vpad/input.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

struct Libs
{
    Libs()
    {
        WHBProcInit();
        WHBLogCafeInit();
        WHBLogUdpInit();
        WHBGfxInit();
        VPADInit();
    }
    ~Libs()
    {
        VPADShutdown();
        WHBGfxShutdown();
        WHBLogUdpDeinit();
        WHBLogCafeDeinit();
        WHBProcShutdown();
    }
};

void ExitToMenu()
{
    SYSLaunchMenu();
    while (WHBProcIsRunning())
    {
    }
}

int main()
{
    Libs libs{};
    auto sdPath = std::filesystem::path(WHBGetSdCardMountPath()) / "wiiu" / "videos" / "videoplayback.mp4";

    H264TrackData trackData{};
    trackData.stream.reserve(23'000'000u);
    if (!LoadAVCTrackFromMP4(sdPath, trackData))
    {
        WHBLogPrint("Failed to load track");
        ExitToMenu();
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

    std::unique_ptr<Gfx> gfx;
    try
    {
        gfx = std::make_unique<Gfx>();
    }
    catch (const std::exception& e)
    {
        WHBLogPrint(e.what());
        ExitToMenu();
        return -1;
    }
    gfx->SetFrameDimensions(trackData.width, trackData.height);
    gfx->SetVideoDrawTargets(Gfx::DrawTargets::TV | Gfx::DrawTargets::DRC);

    std::optional<H264Decoder> decoder;
    try
    {
        decoder.emplace(static_cast<H264Profile>(trackData.profile), trackData.level, trackData.width,
                        trackData.height);
    }
    catch (const std::exception& e)
    {
        WHBLogPrint(e.what());
        ExitToMenu();
        return -1;
    }

    decoder->SubmitFrame(std::span(trackData.stream).subspan(decStartOffset), 0);

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

    return 0;
}
