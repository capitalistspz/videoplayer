#include <iostream>
#include <whb/sdcard.h>
#include <whb/file.h>
#include <whb/log.h>
#include <whb/log_cafe.h>

int main() {
    WHBLogCafeInit();
    uint32_t fileSize;
    auto mp4Data = WHBReadWholeFile("~/wiiu/videos/test.mp4", &fileSize);
    if (!mp4Data) {
        WHBLogPrintf("Failed to read file");
        return -1;
    }
    WHBLogPrintf("Read %u bytes", fileSize);
    WHBLogCafeDeinit();
    return 0;
}
