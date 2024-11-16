#pragma once
#include <exception>
#include <memory>
#include <string>

#include <whb/gfx.h>

#include "H264.h"

class GfxException : public std::exception
{
  public:
    explicit GfxException(std::string_view str);
    explicit GfxException(std::string_view str, std::string_view reason);

    [[nodiscard]] const char* what() const noexcept override;

  private:
    std::string m_content;
};

class Gfx
{
  public:
    enum class DrawTargets
    {
        None = 0,
        TV = 1 << 0,
        DRC = 1 << 1
    };

    // WHBGfxInit and GLSL_Init have to be run before this
    explicit Gfx(const std::string& vertSource, const std::string& fragSource);
    ~Gfx();

    void SetFrameDimensions(unsigned width, unsigned height);

    // NV 12 format frame
    bool SetFrameBuffer(const H264Decoder::OutputFrameInfo& frameInfo);

    // Bitmask
    void SetVideoDrawTargets(DrawTargets targets);

    void Draw();

  private:
    void DrawInternal();

  private:
    GX2Texture m_yTexture{};
    GX2Texture m_uvTexture{};
    GX2Sampler m_ySampler{};
    GX2Sampler m_uvSampler{};
    WHBGfxShaderGroup m_shaderGroup{};
    DrawTargets m_targets{};
    GX2RBuffer m_vtxPosBuffer{};
    GX2RBuffer m_texCoordBuffer{};
};

WUT_ENUM_BITMASK_TYPE(Gfx::DrawTargets);
