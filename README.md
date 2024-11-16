# Video Player
Attempt at a H264 MP4 video player on the Wii U. Currently only displays the first frame.

## Dependencies
- [devkitPPC](https://devkitpro.org/)
- [wut](https://github.com/devkitPro/wut)
- [CafeGLSL](https://github.com/Exzap/CafeGLSL) for shader compilation
- [bento4/ap4](https://github.com/axiomatic-systems/Bento4) for reading MP4 files, use at least commit [4cb5f235f6baec7e8f30e0a8ddafee37049daa5d](https://github.com/axiomatic-systems/Bento4/commit/4cb5f235f6baec7e8f30e0a8ddafee37049daa5d)
- [glm](https://github.com/g-truc/glm)

### Installing GLM
```
# MSYS2/Fedora/Arch users
pacman -S ppc-glm
# Debian/Ubuntu users
dkp-pacman -S ppc-glm
```
### Building Bento4
```
git clone https://github.com/axiomatic-systems/Bento4
cd Bento4
# Configure build
cmake -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake -S . -B out
cd out
# Build
cmake --build .
```
### Installing Bento4
```
# Install lib to $DEVKITPRO/portlibs/wiiu
cmake --install .
```

## Building app
```
cmake -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake -S . -B build
cd build
cmake --build .
```
