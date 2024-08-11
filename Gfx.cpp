#include "Gfx.h"

#include <CafeGLSLCompiler.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/surface.h>
#include <gx2/utils.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <whb/log.h>

GfxException::GfxException(std::string_view str) : m_content(str)
{
    WHBLogPrintf("GfxException: %s", str.data());
}

GfxException::GfxException(std::string_view str, std::string_view reason) : m_content(str)
{
    WHBLogPrintf("GfxException: %s %s", str.data(), reason.data());
}

const char* GfxException::what() const noexcept
{
    return m_content.c_str();
}

static glm::vec2 s_texCoords[4]{{0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 0.0}};

static glm::vec2 s_posCoords[4]{{-1.0, -1.0}, {+1.0, -1.0}, {+1.0, +1.0}, {-1.0, +1.0}};

void CommonTexInit(GX2Texture& tex)
{
    tex.viewNumMips = 1;
    tex.viewNumSlices = 1;
    tex.surface.mipLevels = 1;
    tex.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    tex.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    tex.surface.use = GX2_SURFACE_USE_TEXTURE;
    tex.surface.aa = GX2_AA_MODE1X;
    tex.surface.depth = 1;
}

Gfx::Gfx(const std::string& vertSource, const std::string& fragSource)
{
    constexpr static auto infoLogSize = 512;
    char infoLogBuffer[infoLogSize];
    auto* const vertShader =
        GLSL_CompileVertexShader(vertSource.data(), infoLogBuffer, infoLogSize, GLSL_COMPILER_FLAG_NONE);
    if (!vertShader)
    {
        throw GfxException("Failed to compile vertex shader", infoLogBuffer);
    }
    auto* const pixShader =
        GLSL_CompilePixelShader(fragSource.data(), infoLogBuffer, infoLogSize, GLSL_COMPILER_FLAG_NONE);
    if (!pixShader)
    {
        throw GfxException("Failed to compile pixel shader", infoLogBuffer);
    }
    m_shaderGroup.vertexShader = vertShader;
    m_shaderGroup.pixelShader = pixShader;
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, m_shaderGroup.vertexShader->program,
                  m_shaderGroup.vertexShader->size);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, m_shaderGroup.pixelShader->program, m_shaderGroup.pixelShader->size);

    GX2InitSampler(&m_ySampler, GX2_TEX_CLAMP_MODE_CLAMP_BORDER, GX2_TEX_XY_FILTER_MODE_LINEAR);
    GX2InitSamplerBorderType(&m_ySampler, GX2_TEX_BORDER_TYPE_BLACK);

    // Avoids green outline around image
    GX2InitSampler(&m_uvSampler, GX2_TEX_CLAMP_MODE_CLAMP_BORDER, GX2_TEX_XY_FILTER_MODE_POINT);
    GX2InitSamplerBorderType(&m_uvSampler, GX2_TEX_BORDER_TYPE_BLACK);

    CommonTexInit(m_yTexture);
    CommonTexInit(m_uvTexture);

    // Y -> R
    m_yTexture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
    m_yTexture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8;
    // U -> R, V -> G
    m_uvTexture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
    m_uvTexture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8;

    WHBGfxInitShaderAttribute(&m_shaderGroup, "inPosCoord", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitShaderAttribute(&m_shaderGroup, "inTexCoord", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);

    m_vtxPosBuffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_GPU_READ;
    m_texCoordBuffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_GPU_READ;
    GX2RCreateBufferUserMemory(&m_vtxPosBuffer, s_posCoords, sizeof(s_posCoords));
    GX2RCreateBufferUserMemory(&m_texCoordBuffer, s_texCoords, sizeof(s_texCoords));

    WHBGfxInitFetchShader(&m_shaderGroup);
}

void Gfx::SetFrameDimensions(unsigned width, unsigned height)
{
    m_yTexture.surface.width = width;
    m_yTexture.surface.height = height;
    m_uvTexture.surface.width = width / 2;
    m_uvTexture.surface.height = height / 2;

    GX2CalcSurfaceSizeAndAlignment(&m_yTexture.surface);
    GX2InitTextureRegs(&m_yTexture);

    GX2CalcSurfaceSizeAndAlignment(&m_uvTexture.surface);
    GX2InitTextureRegs(&m_uvTexture);

    if (m_yTexture.surface.image)
        std::free(m_yTexture.surface.image);
    if (m_uvTexture.surface.image)
        std::free(m_yTexture.surface.image);

    m_yTexture.surface.image = std::aligned_alloc(m_yTexture.surface.alignment, m_yTexture.surface.imageSize);
    m_uvTexture.surface.image = std::aligned_alloc(m_uvTexture.surface.alignment, m_uvTexture.surface.imageSize);
}

template <size_t PixelWidth>
void CopyToSurface(GX2Surface& targetSurface, const uint8_t* sourceData, uint32_t sourcePixelPitch)
{
    const auto surfaceImage = static_cast<uint8_t*>(targetSurface.image);
    const auto surfacePitch = targetSurface.pitch == 0 ? targetSurface.width : targetSurface.pitch;

    for (auto line = 0u; line < targetSurface.height; ++line)
    {
        std::memcpy(surfaceImage + line * surfacePitch * PixelWidth, sourceData + line * sourcePixelPitch * PixelWidth,
                    targetSurface.width * PixelWidth);
    }
}

bool Gfx::SetFrameBuffer(const H264Decoder::OutputFrameInfo& frameInfo)
{
    CopyToSurface<1>(m_yTexture.surface, frameInfo.buffer.data(), frameInfo.pitch);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, m_yTexture.surface.image, m_yTexture.surface.imageSize);
    CopyToSurface<2>(m_uvTexture.surface, frameInfo.buffer.data() + frameInfo.height * frameInfo.pitch,
                     frameInfo.pitch / 2);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, m_uvTexture.surface.image, m_uvTexture.surface.imageSize);

    return true;
}

void Gfx::SetVideoDrawTargets(DrawTargets targets)
{
    m_targets = targets;
}

void Gfx::Draw()
{
    WHBGfxBeginRender();
    if ((m_targets & DrawTargets::TV) != DrawTargets::None)
    {
        WHBGfxBeginRenderTV();
        DrawInternal();
        WHBGfxFinishRenderTV();
    }
    if ((m_targets & DrawTargets::DRC) != DrawTargets::None)
    {
        WHBGfxBeginRenderDRC();
        DrawInternal();
        WHBGfxFinishRenderDRC();
    }
    WHBGfxFinishRender();
}

void Gfx::DrawInternal()
{
    WHBGfxClearColor(0.3, 0.3, 0.3, 1.0);

    GX2SetFetchShader(&m_shaderGroup.fetchShader);
    GX2SetVertexShader(m_shaderGroup.vertexShader);
    GX2SetPixelShader(m_shaderGroup.pixelShader);

    GX2SetPixelTexture(&m_yTexture, m_shaderGroup.pixelShader->samplerVars[0].location);
    GX2SetPixelTexture(&m_uvTexture, m_shaderGroup.pixelShader->samplerVars[1].location);

    GX2SetPixelSampler(&m_ySampler, m_shaderGroup.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&m_uvSampler, m_shaderGroup.pixelShader->samplerVars[1].location);

    GX2RSetAttributeBuffer(&m_vtxPosBuffer, 0, sizeof(glm::vec2), 0);
    GX2RSetAttributeBuffer(&m_texCoordBuffer, 1, sizeof(glm::vec2), 0);

    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}
