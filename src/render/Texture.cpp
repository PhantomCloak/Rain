#include "Texture.h"
#include "render/Render.h"
#include "render/RenderContext.h"
#include "render/RenderUtils.h"
#include "render/TextureImporter.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_RESIZE2_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#endif
#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif

#include <stb_image_resize2.h>

void WriteMipLevel(Buffer pixelData, const WGPUTexture& target, uint32_t width, uint32_t height, uint32_t mipCount);

Ref<Texture> Texture::Create(const TextureProps& props) {
  auto textureRef = CreateRef<Texture>(props);
  return textureRef;
}

Ref<Texture> Texture::Create(const TextureProps& props, const std::filesystem::path& path) {
  auto textureRef = CreateRef<Texture>(props, path);
  return textureRef;
}

Texture::Texture() {
}

Texture::Texture(const TextureProps& props)
    : m_TextureProps(props) {
  Invalidate();
}
Texture::Texture(const TextureProps& props, const std::filesystem::path& path)
    : m_TextureProps(props) {
  CreateFromFile(props, path);
}

void Texture::Resize(uint width, uint height) {
  m_TextureProps.Width = width;
  m_TextureProps.Height = height;
  Invalidate();
}

void Texture::Release() {
  wgpuTextureRelease(TextureBuffer);

  for (const auto& view : m_Views) {
    wgpuTextureViewRelease(view);
  }

  if (m_TextureProps.CreateSampler) {
    Sampler->Release();
  }
  m_Views.clear();
}

void Texture::Invalidate() {
  if (TextureBuffer != NULL && m_Views.size() <= 0) {
    wgpuTextureRelease(TextureBuffer);
    for (const auto& view : m_Views) {
      wgpuTextureViewRelease(view);
    }
    m_Views.clear();
  }

  uint32_t mipCount = m_TextureProps.GenerateMips ? RenderUtils::CalculateMipCount(m_TextureProps.Width, m_TextureProps.Height) : 1;

  WGPUTextureDescriptor textureDesc = {};
  textureDesc.nextInChain = nullptr;
  textureDesc.label = m_TextureProps.DebugName.c_str();
  textureDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  textureDesc.dimension = WGPUTextureDimension_2D;
  textureDesc.size.width = m_TextureProps.Width;
  textureDesc.size.height = m_TextureProps.Height;
  textureDesc.size.depthOrArrayLayers = m_TextureProps.layers;

  textureDesc.format = RenderTypeUtils::ToRenderType(m_TextureProps.Format);
  textureDesc.mipLevelCount = mipCount;
  textureDesc.sampleCount = 1;

  if (m_TextureProps.CreateSampler) {
    std::string samplerName = "S_" + m_TextureProps.DebugName;
    SamplerProps samplerProps = {
        .Name = samplerName,
        .WrapFormat = m_TextureProps.SamplerWrap,
        .MagFilterFormat = m_TextureProps.SamplerFilter,
        .MinFilterFormat = m_TextureProps.SamplerFilter,
        .MipFilterFormat = m_TextureProps.SamplerFilter,
        .LodMinClamp = 0.0f,
        .LodMaxClamp = (float)mipCount};

    Sampler = Sampler::Create(samplerProps);
  }

  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();

  TextureBuffer = wgpuDeviceCreateTexture(renderContext->GetDevice(), &textureDesc);

  if (m_ImageData.GetSize() > 0) {
    WriteMipLevel(m_ImageData, TextureBuffer, m_TextureProps.Width, m_TextureProps.Height, mipCount);
  }

	m_Views.clear();
  if (m_TextureProps.layers > 1) {
    WGPUTextureViewDescriptor textureViewDesc = {};
    textureViewDesc.aspect = WGPUTextureAspect_All;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = m_TextureProps.layers;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
    textureViewDesc.dimension = WGPUTextureViewDimension_2DArray;
    textureViewDesc.format = textureDesc.format;
    m_Views.push_back(wgpuTextureCreateView(TextureBuffer, &textureViewDesc));
  }

  for (int i = 0; i < m_TextureProps.layers; i++) {
    WGPUTextureViewDescriptor textureViewDesc = {};
    textureViewDesc.aspect = WGPUTextureAspect_All;
    textureViewDesc.baseArrayLayer = i;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
    textureViewDesc.dimension = WGPUTextureViewDimension_2D;
    textureViewDesc.format = textureDesc.format;

    m_Views.push_back(wgpuTextureCreateView(TextureBuffer, &textureViewDesc));
  }
}

void Texture::CreateFromFile(const TextureProps& props, const std::filesystem::path& path) {
  m_ImageData = TextureImporter::ImportFileToBuffer(path, m_TextureProps.Format, m_TextureProps.Width, m_TextureProps.Height);
  Invalidate();
}

void WriteTexture(void* pixelData, const WGPUTexture& target, uint32_t width, uint32_t height, uint32_t targetMip) {
  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();
  auto queue = renderContext->GetQueue();
  WGPUOrigin3D targetOrigin = {0, 0, 0};

  WGPUImageCopyTexture dest = {
      .texture = target,
      .mipLevel = targetMip,
      .origin = targetOrigin,
      .aspect = WGPUTextureAspect_All};

  WGPUTextureDataLayout textureLayout = {
      .offset = 0,
      .bytesPerRow = 4 * width,
      .rowsPerImage = height};

  WGPUExtent3D textureSize = {.width = width, .height = height, .depthOrArrayLayers = 1};
  wgpuQueueWriteTexture(*queue, &dest, pixelData, (4 * width * height), &textureLayout, &textureSize);
}

void WriteMipLevel(Buffer pixelData, const WGPUTexture& target, uint32_t width, uint32_t height, uint32_t mipCount) {
  WriteTexture(pixelData.Data, target, width, height, 0);

  std::vector<unsigned char> prevPixelBuffer(pixelData.Size);
  std::memcpy(prevPixelBuffer.data(), pixelData.Data, pixelData.Size);

  uint32_t prevWidth = width, prevHeight = height;
  uint32_t currentWidth = width;
  uint32_t currentHeight = height;

  for (uint32_t mipLevel = 1; mipLevel < mipCount; mipLevel++) {
    currentWidth = currentWidth > 1 ? currentWidth / 2 : 1;
    currentHeight = currentHeight > 1 ? currentHeight / 2 : 1;

    std::vector<unsigned char> curPixelBuffer(4 * currentWidth * currentHeight);

    stbir_resize_uint8_linear(prevPixelBuffer.data(),
                              prevWidth,
                              prevHeight,
                              0,
                              curPixelBuffer.data(),
                              currentWidth,
                              currentHeight,
                              0,
                              STBIR_RGBA);

    WriteTexture(curPixelBuffer.data(), target, currentWidth, currentHeight, mipLevel);

    prevWidth = currentWidth;
    prevHeight = currentHeight;

    prevPixelBuffer = std::move(curPixelBuffer);
  }
}
