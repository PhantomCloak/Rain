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
void WriteTexture(void* pixelData, const WGPUTexture& target, uint32_t width, uint32_t height, uint32_t targetMip, uint32_t targetLayer = 0);

Ref<Texture2D> Texture2D::Create(const TextureProps& props) {
  auto textureRef = CreateRef<Texture2D>(props);
  return textureRef;
}

Ref<Texture2D> Texture2D::Create(const TextureProps& props, const std::filesystem::path& path) {
  auto textureRef = CreateRef<Texture2D>(props, path);
  return textureRef;
}

Texture2D::Texture2D() {
}

Texture2D::Texture2D(const TextureProps& props)
    : m_TextureProps(props) {
  Invalidate();
}
Texture2D::Texture2D(const TextureProps& props, const std::filesystem::path& path)
    : m_TextureProps(props) {
  CreateFromFile(props, path);
}

void Texture2D::Resize(uint width, uint height) {
  m_TextureProps.Width = width;
  m_TextureProps.Height = height;
  Invalidate();
}

void Texture2D::Release() {
  wgpuTextureRelease(TextureBuffer);

  for (const auto& view : m_Views) {
    wgpuTextureViewRelease(view);
  }

  if (m_TextureProps.CreateSampler) {
    Sampler->Release();
  }
  m_Views.clear();
}

void Texture2D::Invalidate() {
  if (TextureBuffer != NULL && m_Views.size() <= 0) {
    wgpuTextureRelease(TextureBuffer);
    for (const auto& view : m_Views) {
      wgpuTextureViewRelease(view);
    }
    m_Views.clear();
  }

  uint32_t mipCount = m_TextureProps.GenerateMips ? RenderUtils::CalculateMipCount(m_TextureProps.Width, m_TextureProps.Height) : 1;

  WGPUTextureDescriptor textureDesc = {};
  ZERO_INIT(textureDesc);

  textureDesc.nextInChain = nullptr;
  textureDesc.label = m_TextureProps.DebugName.c_str();

  if (m_TextureProps.GenerateMips) {
    textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
  } else {
    textureDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  }

  textureDesc.dimension = WGPUTextureDimension_2D;
  textureDesc.size.width = m_TextureProps.Width;
  textureDesc.size.height = m_TextureProps.Height;
  textureDesc.size.depthOrArrayLayers = m_TextureProps.layers;
  textureDesc.sampleCount = m_TextureProps.MultiSample;

  textureDesc.format = RenderTypeUtils::ToRenderType(m_TextureProps.Format);
  textureDesc.mipLevelCount = mipCount;
  textureDesc.sampleCount = m_TextureProps.MultiSample;

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
    WriteTexture(m_ImageData.Data, TextureBuffer, m_TextureProps.Width, m_TextureProps.Height, 0);
  }

  m_Views.clear();
  if (m_TextureProps.layers > 1) {
    WGPUTextureViewDescriptor textureViewDesc = {};
    ZERO_INIT(textureViewDesc);
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
    ZERO_INIT(textureViewDesc);
    textureViewDesc.aspect = WGPUTextureAspect_All;
    textureViewDesc.baseArrayLayer = i;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
    textureViewDesc.dimension = WGPUTextureViewDimension_2D;
    textureViewDesc.format = textureDesc.format;

    m_Views.push_back(wgpuTextureCreateView(TextureBuffer, &textureViewDesc));
  }

  if (m_TextureProps.GenerateMips) {
    Render::ComputeMip(this);
  }
}

void Texture2D::CreateFromFile(const TextureProps& props, const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    std::cerr << "Texture file not found: " << path << std::endl;
    return;
  }
  m_ImageData = TextureImporter::ImportFileToBuffer(path, m_TextureProps.Format, m_TextureProps.Width, m_TextureProps.Height);
  Invalidate();
}

Ref<TextureCube> TextureCube::Create(const TextureProps& props, const std::filesystem::path (&paths)[6]) {
  auto textureRef = CreateRef<TextureCube>(props, paths);
  return textureRef;
}

TextureCube::TextureCube(const TextureProps& props, const std::filesystem::path (&path)[6])
    : m_TextureProps(props) {
  CreateFromFile(props, path);
}

void TextureCube::CreateFromFile(const TextureProps& props, const std::filesystem::path (&paths)[6]) {
  for (int i = 0; i < 6; i++) {
    auto& path = paths[i];
    if (!std::filesystem::exists(path)) {
      std::cerr << "Texture file not found: " << path << std::endl;
      return;
    }

    m_ImageData[i] = TextureImporter::ImportFileToBuffer(path, m_TextureProps.Format, m_TextureProps.Width, m_TextureProps.Height);
  }
  Invalidate();
}

void TextureCube::Invalidate() {
  uint32_t mipCount = m_TextureProps.GenerateMips ? RenderUtils::CalculateMipCount(m_TextureProps.Width, m_TextureProps.Height) : 1;
	WGPUExtent3D cubemapSize = {m_TextureProps.Width, m_TextureProps.Height, 6};

  WGPUTextureDescriptor textureDesc;
  textureDesc.label = "MMB";
  textureDesc.dimension = WGPUTextureDimension_2D;
  textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
  // textureDesc.format = RenderTypeUtils::ToRenderType(m_TextureProps.Format);
  textureDesc.size = cubemapSize;
  textureDesc.mipLevelCount = (uint32_t)(floor((float)(log2(glm::max(textureDesc.size.width, textureDesc.size.height))))) + 1;  // can be replaced with bit_width in C++ 20
  textureDesc.sampleCount = 1;
  //textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
  textureDesc.viewFormatCount = 0;
  textureDesc.viewFormats = nullptr;
  textureDesc.nextInChain = nullptr;
  m_TextureBuffer = wgpuDeviceCreateTexture(RenderContext::GetDevice(), &textureDesc);

  WGPUExtent3D cubemapLayerSize = {cubemapSize.width, cubemapSize.height, 1};
  for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex) {
    WGPUOrigin3D origin = {0, 0, faceIndex};
    WriteTexture(m_ImageData[faceIndex].Data, m_TextureBuffer, m_TextureProps.Width, m_TextureProps.Height, 0, faceIndex);
  }

  WGPUTextureViewDescriptor textureViewDesc;
  textureViewDesc.label = "MMB_View";
  textureViewDesc.aspect = WGPUTextureAspect_All;
  textureViewDesc.baseArrayLayer = 0;
  textureViewDesc.arrayLayerCount = 6;
  textureViewDesc.baseMipLevel = 0;
  textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
  textureViewDesc.dimension = WGPUTextureViewDimension_Cube;
  textureViewDesc.format = textureDesc.format;
  textureViewDesc.nextInChain = nullptr;

  m_Views.push_back(wgpuTextureCreateView(m_TextureBuffer, &textureViewDesc));

  if (m_TextureProps.GenerateMips) {
    Render::PreFilter(this);
  }
}

void WriteTexture(void* pixelData, const WGPUTexture& target, uint32_t width, uint32_t height, uint32_t targetMip, uint32_t targetLayer) {
  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();
  auto queue = renderContext->GetQueue();
  WGPUOrigin3D targetOrigin = {0, 0, targetLayer};

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
