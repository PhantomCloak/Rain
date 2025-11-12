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

namespace Rain {

  void WriteTexture(const void* pixelData, WGPUTexture target, uint32_t width, uint32_t height, uint32_t targetMip, uint32_t targetLayer, TextureFormat format);

  Texture2D::Texture2D() {
  }

  Ref<Texture2D> Texture2D::Create(const TextureProps& props) {
    auto textureRef = CreateRef<Texture2D>(props);
    return textureRef;
  }

  Ref<Texture2D> Texture2D::Create(const TextureProps& props, const std::filesystem::path& path) {
    auto textureRef = CreateRef<Texture2D>(props, path);
    return textureRef;
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
    Invalidate(); }

  void Texture2D::Release() {
    wgpuTextureRelease(TextureBuffer);

    for (const auto& view : m_ReadViews) {
      wgpuTextureViewRelease(view);
    }

    if (m_TextureProps.CreateSampler) {
      Sampler->Release();
    }
    m_ReadViews.clear();
  }

  void Texture2D::Invalidate() {
    if (TextureBuffer != NULL && m_ReadViews.size() <= 0) {
      wgpuTextureRelease(TextureBuffer);
      for (const auto& view : m_ReadViews) {
        wgpuTextureViewRelease(view);
      }
      m_ReadViews.clear();
    }

    uint32_t mipCount = 1;
    if (m_TextureProps.GenerateMips) {
      mipCount = RenderUtils::CalculateMipCount(m_TextureProps.Width, m_TextureProps.Height);
    }

    WGPUTextureDescriptor textureDesc = {};
    ZERO_INIT(textureDesc);

    textureDesc.nextInChain = nullptr;
    textureDesc.label = RenderUtils::MakeLabel(m_TextureProps.DebugName);

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

    TextureBuffer = wgpuDeviceCreateTexture(Render::Instance->GetRenderContext()->GetDevice(), &textureDesc);

    if (m_ImageData.GetSize() > 0) {
      WriteTexture(m_ImageData.Data, TextureBuffer, m_TextureProps.Width, m_TextureProps.Height, 0, 0, m_TextureProps.Format);
    }

    m_ReadViews.clear();
    WGPUTextureViewDescriptor viewDesc = {};
    ZERO_INIT(viewDesc);
    viewDesc.aspect = WGPUTextureAspect_All;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = m_TextureProps.layers;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = textureDesc.mipLevelCount;
    viewDesc.dimension = WGPUTextureViewDimension_2DArray;
    viewDesc.format = textureDesc.format;

    // Create the appropriate primary view
    if (m_TextureProps.layers > 1) {
      // Array texture: create one 2D array view
      viewDesc.dimension = WGPUTextureViewDimension_2DArray;
      viewDesc.baseArrayLayer = 0;
      viewDesc.arrayLayerCount = m_TextureProps.layers;
      viewDesc.baseMipLevel = 0;
      viewDesc.mipLevelCount = mipCount;
      m_ReadViews.push_back(wgpuTextureCreateView(TextureBuffer, &viewDesc));
    }

    // Create individual views based on texture type
    if (m_TextureProps.GenerateMips) {
      // For mipmapped textures: create a view for each mip level
      for (uint32_t mip = 0; mip < mipCount; mip++) {
        viewDesc.dimension = WGPUTextureViewDimension_2D;
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1;
        viewDesc.baseMipLevel = mip;
        viewDesc.mipLevelCount = 1;

        WGPUTextureView view = wgpuTextureCreateView(TextureBuffer, &viewDesc);
        m_ReadViews.push_back(view);
        m_WriteViews.push_back(view);  // Same view for both read/write
      }
    } else if (m_TextureProps.layers > 1) {
      // For array textures without mipmaps: create a view for each layer
      for (uint32_t layer = 0; layer < m_TextureProps.layers; layer++) {
        viewDesc.dimension = WGPUTextureViewDimension_2D;
        viewDesc.baseArrayLayer = layer;
        viewDesc.arrayLayerCount = 1;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipLevelCount = 1;

        WGPUTextureView view = wgpuTextureCreateView(TextureBuffer, &viewDesc);
        m_ReadViews.push_back(view);
        m_WriteViews.push_back(view);  // Same view for both read/write
      }
    } else {
      // Simple 2D texture: just create one view
      viewDesc.dimension = WGPUTextureViewDimension_2D;
      viewDesc.baseArrayLayer = 0;
      viewDesc.arrayLayerCount = 1;
      viewDesc.baseMipLevel = 0;
      viewDesc.mipLevelCount = 1;

      WGPUTextureView view = wgpuTextureCreateView(TextureBuffer, &viewDesc);
      m_ReadViews.push_back(view);
      m_WriteViews.push_back(view);
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

  TextureCube::TextureCube(const TextureProps& props, const std::filesystem::path (&path)[6])
      : m_TextureProps(props) {
    CreateFromFile(props, path);
  }

  TextureCube::TextureCube(const TextureProps& props)
      : m_TextureProps(props) {
    Invalidate();
  }

  Ref<TextureCube> TextureCube::Create(const TextureProps& props) {
    auto textureRef = CreateRef<TextureCube>(props);
    return textureRef;
  }

  Ref<TextureCube> TextureCube::Create(const TextureProps& props, const std::filesystem::path (&paths)[6]) {
    auto textureRef = CreateRef<TextureCube>(props, paths);
    return textureRef;
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
    uint32_t mipCount = 1;
    if (m_TextureProps.GenerateMips) {
      mipCount = RenderUtils::CalculateMipCount(m_TextureProps.Width, m_TextureProps.Height);
    }
    WGPUExtent3D cubemapSize = {m_TextureProps.Width, m_TextureProps.Height, 6};

    WGPUTextureDescriptor imageDesc;
    imageDesc.label = RenderUtils::MakeLabel(m_TextureProps.DebugName);
    imageDesc.dimension = WGPUTextureDimension_2D;
    imageDesc.format = RenderTypeUtils::ToRenderType(m_TextureProps.Format);
    imageDesc.size = cubemapSize;
    imageDesc.mipLevelCount = mipCount;
    imageDesc.sampleCount = 1;
    imageDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;

    imageDesc.viewFormatCount = 0;
    imageDesc.viewFormats = nullptr;
    imageDesc.nextInChain = nullptr;

    m_TextureBuffer = wgpuDeviceCreateTexture(RenderContext::GetDevice(), &imageDesc);

    WGPUExtent3D cubemapLayerSize = {cubemapSize.width, cubemapSize.height, 1};
    for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex) {
      WGPUOrigin3D origin = {0, 0, faceIndex};
      if (m_ImageData[faceIndex].Size > 0) {
        WriteTexture(m_ImageData[faceIndex].Data, m_TextureBuffer, m_TextureProps.Width, m_TextureProps.Height, 0, faceIndex, m_TextureProps.Format);
      }
    }

    WGPUTextureViewDescriptor arrayViewDesc;
    arrayViewDesc.label = RenderUtils::MakeLabel("MMB_View");
    arrayViewDesc.aspect = WGPUTextureAspect_All;
    arrayViewDesc.baseArrayLayer = 0;                         // Start from the first array layer (face)
    arrayViewDesc.arrayLayerCount = 6;                        // Cubemap has 6 faces
    arrayViewDesc.baseMipLevel = 0;                           // Start from the base mip level
    arrayViewDesc.mipLevelCount = mipCount;                   // Include all mip levels
    arrayViewDesc.usage = imageDesc.usage;                    // Use the same usage as the texture
    arrayViewDesc.dimension = WGPUTextureViewDimension_Cube;  // View as a cubemap
    arrayViewDesc.format = RenderTypeUtils::ToRenderType(m_TextureProps.Format);
    arrayViewDesc.nextInChain = nullptr;

    m_ReadViews.push_back(wgpuTextureCreateView(m_TextureBuffer, &arrayViewDesc));

    for (uint32_t mipLevel = 0; mipLevel < mipCount; mipLevel++) {
      arrayViewDesc.dimension = WGPUTextureViewDimension_2DArray;
      arrayViewDesc.baseMipLevel = mipLevel;
      arrayViewDesc.mipLevelCount = 1;
      m_WriteViews.push_back(wgpuTextureCreateView(m_TextureBuffer, &arrayViewDesc));
    }
  }

  void WriteTexture(const void* pixelData, WGPUTexture target, uint32_t width, uint32_t height, uint32_t targetMip, uint32_t targetLayer, TextureFormat format) {
    if (!pixelData || !target) {
      RN_LOG_ERR("WriteTexture: Invalid pixel data or texture target");
      return;
    }

    if (width == 0 || height == 0) {
      RN_LOG_ERR("WriteTexture: Invalid dimensions (width: {}, height: {})", width, height);
      return;
    }

    uint32_t bytesPerPixel = TextureUtils::GetBytesPerPixel(format);
    if (bytesPerPixel == 0) {
      RN_LOG_ERR("WriteTexture: Unsupported format {}", (int)format);
      return;
    }

    Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();
    auto queue = renderContext->GetQueue();
    if (!queue) {
      RN_LOG_ERR("WriteTexture: Invalid render queue");
      return;
    }

    WGPUOrigin3D targetOrigin = {0, 0, targetLayer};

    WGPUTexelCopyTextureInfo dest = {
        .texture = target,
        .mipLevel = targetMip,
        .origin = targetOrigin,
        .aspect = WGPUTextureAspect_All};

    uint32_t unalignedBytesPerRow = bytesPerPixel * width;
    uint32_t alignedBytesPerRow = (unalignedBytesPerRow + 255) & ~255;  // Align to 256 bytes

    WGPUTexelCopyBufferLayout textureLayout = {
        .offset = 0,
        .bytesPerRow = alignedBytesPerRow,
        .rowsPerImage = height};

    WGPUExtent3D textureSize = {
        .width = width,
        .height = height,
        .depthOrArrayLayers = 1};

    if (unalignedBytesPerRow != alignedBytesPerRow) {
      size_t alignedDataSize = alignedBytesPerRow * height;
      std::vector<uint8_t> alignedData(alignedDataSize, 0);

      const uint8_t* srcData = static_cast<const uint8_t*>(pixelData);
      for (uint32_t y = 0; y < height; y++) {
        memcpy(
            alignedData.data() + y * alignedBytesPerRow,
            srcData + y * unalignedBytesPerRow,
            unalignedBytesPerRow);
      }

      wgpuQueueWriteTexture(*queue, &dest, alignedData.data(), alignedDataSize, &textureLayout, &textureSize);
    } else {
      size_t dataSize = alignedBytesPerRow * height;
      wgpuQueueWriteTexture(*queue, &dest, pixelData, dataSize, &textureLayout, &textureSize);
    }
  }
}  // namespace Rain
