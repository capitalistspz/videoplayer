#pragma once
#include <vector>
#include <filesystem>


bool LoadAVCTrackFromMP4(const std::filesystem::path &path, std::vector<uint8_t> &byteStream, unsigned &outWidth,
                         unsigned &outHeight);
