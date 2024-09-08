#pragma once
#include <filesystem>
#include <vector>

struct H264TrackData
{
    std::vector<uint8_t> stream;
    std::vector<size_t> sampleOffsets;
    unsigned width;
    unsigned height;
    unsigned profile;
    unsigned level;
};

bool LoadAVCTrackFromMP4(const std::filesystem::path& path, H264TrackData& data);
