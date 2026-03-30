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

namespace WebEngine
{

  void WriteTexture(const void* pixelData, WGPUTexture target, uint32_t width, uint32_t height, uint32_t targetMip, uint32_t targetLayer, TextureFormat format);

  Texture2D::Texture2D()
  {
  }

  Ref<Texture2D> Texture2D::Create(const TextureProps& props)
  {
    auto textureRef = CreateRef<Texture2D>(props);
    return textureRef;
  }

  Ref<Texture2D> Texture2D::Create(const TextureProps& props, const std::filesystem::path& path)
  {
    auto textureRef = CreateRef<Texture2D>(props, path);
    return textureRef;
  }

  Ref<Texture2D> Texture2D::CreateFromMemory(const TextureProps& props, Buffer imageData)
  {
    auto tex = CreateRef<Texture2D>();
    tex->m_TextureProps = props;
    tex->m_ImageData    = imageData;
    tex->Invalidate();
    return tex;
  }

  Ref<Texture2D> Texture2D::CreateFromKTX(const TextureProps& props, KTXImportResult& ktxData)
  {
    auto tex = CreateRef<Texture2D>();
    tex->m_TextureProps = props;

    const bool isCompressed = TextureUtils::IsBlockCompressed(props.Format);
    uint32_t mipCount = static_cast<uint32_t>(ktxData.mips.size());

    bool needsComputeMip = false;
    if (mipCount == 1 && !isCompressed && props.GenerateMips)
    {
      mipCount = RenderUtils::CalculateMipCount(props.Width, props.Height);
      needsComputeMip = true;
    }

    WGPUTextureDescriptor textureDesc = {};
    ZERO_INIT(textureDesc);
    textureDesc.nextInChain = nullptr;
    textureDesc.label = RenderUtils::MakeLabel(props.DebugName);

    if (isCompressed)
    {
      textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    }
    else if (needsComputeMip)
    {
      textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
    }
    else
    {
      textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    }

    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.size.width  = isCompressed ? (props.Width + 3) & ~3u : props.Width;
    textureDesc.size.height = isCompressed ? (props.Height + 3) & ~3u : props.Height;
    textureDesc.size.depthOrArrayLayers = 1;
    textureDesc.sampleCount = 1;
    textureDesc.format = RenderTypeUtils::ToRenderType(props.Format);
    textureDesc.mipLevelCount = mipCount;

    if (props.CreateSampler)
    {
      std::string samplerName = "S_" + props.DebugName;
      SamplerProps samplerProps = {
          .Name = samplerName,
          .WrapFormat = props.SamplerWrap,
          .MagFilterFormat = props.SamplerFilter,
          .MinFilterFormat = props.SamplerFilter,
          .MipFilterFormat = props.SamplerFilter,
          .LodMinClamp = 0.0f,
          .LodMaxClamp = (float)mipCount};
      tex->Sampler = Sampler::Create(samplerProps);
    }

    auto* Renderer = Render::Get();
    auto renderContext = Renderer->GetRenderContext();
    WGPUDevice device = renderContext->GetDevice();
    const Ref<WGPUQueue> queue = renderContext->GetQueue();

    tex->TextureBuffer = wgpuDeviceCreateTexture(device, &textureDesc);

    // Batch all mip uploads into a single staging buffer + single submit
    uint32_t ktxMipCount = static_cast<uint32_t>(ktxData.mips.size());
    uint32_t blockSize = isCompressed
        ? TextureUtils::GetBytesPerBlock(props.Format)
        : TextureUtils::GetBytesPerPixel(props.Format);

    struct StagedMip {
      uint64_t bufferOffset;
      uint32_t bytesPerRow;
      uint32_t rowCount;
      uint32_t copyWidth;
      uint32_t copyHeight;
    };

    std::vector<StagedMip> stagedMips(ktxMipCount);
    uint64_t totalStagingSize = 0;

    for (uint32_t i = 0; i < ktxMipCount; i++)
    {
      const auto& mip = ktxData.mips[i];
      auto& staged = stagedMips[i];

      if (isCompressed)
      {
        uint32_t blockCols = (mip.width + 3) / 4;
        uint32_t blockRows = (mip.height + 3) / 4;
        staged.bytesPerRow = ((blockCols * blockSize) + 255) & ~255u;
        staged.rowCount    = blockRows;
        staged.copyWidth   = (mip.width + 3) & ~3u;
        staged.copyHeight  = (mip.height + 3) & ~3u;
      }
      else
      {
        staged.bytesPerRow = ((blockSize * mip.width) + 255) & ~255u;
        staged.rowCount    = mip.height;
        staged.copyWidth   = mip.width;
        staged.copyHeight  = mip.height;
      }

      totalStagingSize = (totalStagingSize + 255) & ~255ull;
      staged.bufferOffset = totalStagingSize;
      totalStagingSize += static_cast<uint64_t>(staged.bytesPerRow) * staged.rowCount;
    }

    // Create staging buffer mapped at creation
    WGPUBufferDescriptor bufDesc = {};
    ZERO_INIT(bufDesc);
    bufDesc.label = RenderUtils::MakeLabel("ktx_staging");
    bufDesc.size  = totalStagingSize;
    bufDesc.usage = WGPUBufferUsage_CopySrc;
    bufDesc.mappedAtCreation = true;

    WGPUBuffer stagingBuffer = wgpuDeviceCreateBuffer(device, &bufDesc);
    uint8_t* mapped = static_cast<uint8_t*>(
        wgpuBufferGetMappedRange(stagingBuffer, 0, totalStagingSize));

    // Pack all mip data into the staging buffer
    for (uint32_t i = 0; i < ktxMipCount; i++)
    {
      const auto& mip = ktxData.mips[i];
      const auto& staged = stagedMips[i];
      const uint8_t* srcData = static_cast<const uint8_t*>(ktxData.data.Data) + mip.offset;

      uint32_t srcBytesPerRow = isCompressed
          ? ((mip.width + 3) / 4) * blockSize
          : blockSize * mip.width;

      if (srcBytesPerRow == staged.bytesPerRow)
      {
        memcpy(mapped + staged.bufferOffset, srcData,
               static_cast<size_t>(srcBytesPerRow) * staged.rowCount);
      }
      else
      {
        for (uint32_t row = 0; row < staged.rowCount; row++)
        {
          memcpy(mapped + staged.bufferOffset + static_cast<size_t>(row) * staged.bytesPerRow,
                 srcData + static_cast<size_t>(row) * srcBytesPerRow,
                 srcBytesPerRow);
        }
      }
    }

    wgpuBufferUnmap(stagingBuffer);
    ktxData.data.Release();

    // Single command encoder for all mip copies
    WGPUCommandEncoderDescriptor encDesc = {};
    ZERO_INIT(encDesc);
    encDesc.label = RenderUtils::MakeLabel("ktx_upload");
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encDesc);

    for (uint32_t i = 0; i < ktxMipCount; i++)
    {
      const auto& staged = stagedMips[i];

#ifdef __EMSCRIPTEN__
      WGPUImageCopyBuffer src = {};
      src.nextInChain = nullptr;
      src.layout.offset       = staged.bufferOffset;
      src.layout.bytesPerRow  = staged.bytesPerRow;
      src.layout.rowsPerImage = staged.rowCount;
      src.buffer = stagingBuffer;

      WGPUImageCopyTexture dst = {};
      dst.texture  = tex->TextureBuffer;
      dst.mipLevel = i;
      dst.origin   = {0, 0, 0};
      dst.aspect   = WGPUTextureAspect_All;
#else
      WGPUTexelCopyBufferInfo src = {};
      src.layout.offset       = staged.bufferOffset;
      src.layout.bytesPerRow  = staged.bytesPerRow;
      src.layout.rowsPerImage = staged.rowCount;
      src.buffer = stagingBuffer;

      WGPUTexelCopyTextureInfo dst = {};
      dst.texture  = tex->TextureBuffer;
      dst.mipLevel = i;
      dst.origin   = {0, 0, 0};
      dst.aspect   = WGPUTextureAspect_All;
#endif

      WGPUExtent3D copySize = {staged.copyWidth, staged.copyHeight, 1};
      wgpuCommandEncoderCopyBufferToTexture(encoder, &src, &dst, &copySize);
    }

    WGPUCommandBufferDescriptor cmdDesc = {};
    ZERO_INIT(cmdDesc);
    cmdDesc.label = RenderUtils::MakeLabel("ktx_upload_cmd");
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cmdDesc);
    wgpuQueueSubmit(*queue, 1, &cmdBuf);

    wgpuCommandBufferRelease(cmdBuf);
    wgpuCommandEncoderRelease(encoder);
    wgpuBufferRelease(stagingBuffer);

    // Create views for all mip levels
    for (uint32_t mip = 0; mip < mipCount; mip++)
    {
      WGPUTextureViewDescriptor viewMipDesc = {};
      viewMipDesc.dimension = WGPUTextureViewDimension_2D;
      viewMipDesc.aspect = WGPUTextureAspect_All;
      viewMipDesc.baseArrayLayer = 0;
      viewMipDesc.arrayLayerCount = 1;
      viewMipDesc.baseMipLevel = mip;
      viewMipDesc.mipLevelCount = 1;
      viewMipDesc.format = textureDesc.format;

      WGPUTextureView view = wgpuTextureCreateView(tex->TextureBuffer, &viewMipDesc);
      tex->m_ReadViews.push_back(view);
      tex->m_WriteViews.push_back(view);
    }

    // Fall back to compute mip gen only if KTX had 1 level + uncompressed
    if (needsComputeMip)
    {
      Renderer->ComputeMip(tex.get());
    }

    return tex;
  }

  Texture2D::~Texture2D()
  {
    Release();
  }

  Texture2D::Texture2D(const TextureProps& props)
      : m_TextureProps(props)
  {
    Invalidate();
  }
  Texture2D::Texture2D(const TextureProps& props, const std::filesystem::path& path)
      : m_TextureProps(props)
  {
    CreateFromFile(props, path);
  }

  void Texture2D::Resize(uint width, uint height)
  {
    m_TextureProps.Width = width;
    m_TextureProps.Height = height;
    Invalidate();
  }

  void Texture2D::Release()
  {
    if (TextureBuffer)
    {
      for (const auto& view : m_ReadViews)
      {
        wgpuTextureViewRelease(view);
      }
      m_ReadViews.clear();
      m_WriteViews.clear();
      wgpuTextureRelease(TextureBuffer);
      TextureBuffer = NULL;
    }
    m_ImageData.Release();
  }

  void Texture2D::Invalidate()
  {
    if (TextureBuffer != NULL)
    {
      for (const auto& view : m_ReadViews)
      {
        wgpuTextureViewRelease(view);
      }
      m_ReadViews.clear();
      m_WriteViews.clear();
      wgpuTextureRelease(TextureBuffer);
      TextureBuffer = NULL;
    }

    const bool isCompressed = TextureUtils::IsBlockCompressed(m_TextureProps.Format);

    uint32_t mipCount = 1;
    if (m_TextureProps.GenerateMips && !isCompressed)
    {
      mipCount = RenderUtils::CalculateMipCount(m_TextureProps.Width, m_TextureProps.Height);
    }

    WGPUTextureDescriptor textureDesc = {};
    ZERO_INIT(textureDesc);

    textureDesc.nextInChain = nullptr;
    textureDesc.label = RenderUtils::MakeLabel(m_TextureProps.DebugName);

    if (isCompressed)
    {
      // Block-compressed formats cannot be storage textures or render attachments
      textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    }
    else if (m_TextureProps.GenerateMips)
    {
      textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
    }
    else
    {
      textureDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    }

    textureDesc.dimension = WGPUTextureDimension_2D;
    // Block-compressed textures require dimensions to be multiples of block size (4)
    textureDesc.size.width  = isCompressed ? (m_TextureProps.Width  + 3) & ~3u : m_TextureProps.Width;
    textureDesc.size.height = isCompressed ? (m_TextureProps.Height + 3) & ~3u : m_TextureProps.Height;
    textureDesc.size.depthOrArrayLayers = m_TextureProps.Layers;
    textureDesc.sampleCount = m_TextureProps.MultiSample;
    textureDesc.format = RenderTypeUtils::ToRenderType(m_TextureProps.Format);
    textureDesc.mipLevelCount = mipCount;

    if (m_TextureProps.CreateSampler)
    {
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

    auto* Renderer = Render::Get();
    if (auto renderContext = Renderer->GetRenderContext())
    {
      TextureBuffer = wgpuDeviceCreateTexture(renderContext->GetDevice(), &textureDesc);
    }

    if (m_ImageData.GetSize() > 0)
    {
      // We only write for mip 0 then generate mips in the compute pass
      WriteTexture(m_ImageData.Data, TextureBuffer, m_TextureProps.Width, m_TextureProps.Height, 0, 0, m_TextureProps.Format);
      m_ImageData.Release();
    }

    RN_ASSERT(!(m_TextureProps.Layers > 1 && m_TextureProps.GenerateMips), "Not Implemented Yet");

    // If Layers more than one we put ArrayView instead of the plain view
    if (m_TextureProps.Layers > 1)
    {
      WGPUTextureViewDescriptor viewLayerDesc = {};
      viewLayerDesc.dimension = WGPUTextureViewDimension_2DArray;
      viewLayerDesc.baseArrayLayer = 0;
      viewLayerDesc.arrayLayerCount = m_TextureProps.Layers;
      viewLayerDesc.baseMipLevel = 0;
      viewLayerDesc.mipLevelCount = m_TextureProps.GenerateMips ? mipCount : 1;
      viewLayerDesc.format = textureDesc.format;

      WGPUTextureView viewLayer = wgpuTextureCreateView(TextureBuffer, &viewLayerDesc);
      m_ReadViews.push_back(viewLayer);

      for (uint32_t layer = 0; layer < m_TextureProps.Layers; layer++)
      {
        WGPUTextureViewDescriptor viewLayerDesc = {};
        viewLayerDesc.dimension = WGPUTextureViewDimension_2D;
        viewLayerDesc.aspect = (m_TextureProps.Format == TextureFormat::Depth24Plus)
                                   ? WGPUTextureAspect_DepthOnly
                                   : WGPUTextureAspect_All;
        viewLayerDesc.baseArrayLayer = layer;
        viewLayerDesc.arrayLayerCount = 1;
        viewLayerDesc.baseMipLevel = 0;
        viewLayerDesc.mipLevelCount = 1;
        viewLayerDesc.format = textureDesc.format;

        WGPUTextureView view = wgpuTextureCreateView(TextureBuffer, &viewLayerDesc);
        m_ReadViews.push_back(view);
        m_WriteViews.push_back(view);  // Same view for both read/write
      }
    }
    else
    {
      for (uint32_t mip = 0; mip < mipCount; mip++)
      {
        WGPUTextureViewDescriptor viewMipDesc = {};
        viewMipDesc.dimension = WGPUTextureViewDimension_2D;
        viewMipDesc.aspect = (m_TextureProps.Format == TextureFormat::Depth24Plus)
                                 ? WGPUTextureAspect_DepthOnly
                                 : WGPUTextureAspect_All;
        viewMipDesc.baseArrayLayer = 0;
        viewMipDesc.arrayLayerCount = 1;
        viewMipDesc.baseMipLevel = mip;
        viewMipDesc.mipLevelCount = 1;
        viewMipDesc.format = textureDesc.format;

        WGPUTextureView view = wgpuTextureCreateView(TextureBuffer, &viewMipDesc);
        m_ReadViews.push_back(view);
        m_WriteViews.push_back(view);  // Same view for both read/write
      }

      if (m_TextureProps.GenerateMips && !isCompressed)
      {
        Renderer->ComputeMip(this);
      }
    }
  }

  void Texture2D::CreateFromFile(const TextureProps& props, const std::filesystem::path& path)
  {
    if (!std::filesystem::exists(path))
    {
      std::cerr << "Texture file not found: " << path << '\n';
      return;
    }
    m_ImageData = TextureImporter::ImportFileToBuffer(path, m_TextureProps.Format, m_TextureProps.Width, m_TextureProps.Height);
    Invalidate();
  }

  TextureCube::~TextureCube()
  {
    if (m_TextureBuffer)
    {
      for (const auto& view : m_ReadViews)
      {
        wgpuTextureViewRelease(view);
      }
      for (const auto& view : m_WriteViews)
      {
        wgpuTextureViewRelease(view);
      }
      m_ReadViews.clear();
      m_WriteViews.clear();
      wgpuTextureRelease(m_TextureBuffer);
      m_TextureBuffer = NULL;
    }
    for (auto& buf : m_ImageData)
    {
      buf.Release();
    }
  }

  TextureCube::TextureCube(const TextureProps& props, const std::filesystem::path (&path)[6])
      : m_TextureProps(props)
  {
    CreateFromFile(props, path);
  }

  TextureCube::TextureCube(const TextureProps& props)
      : m_TextureProps(props)
  {
    Invalidate();
  }

  Ref<TextureCube> TextureCube::Create(const TextureProps& props)
  {
    auto textureRef = CreateRef<TextureCube>(props);
    return textureRef;
  }

  Ref<TextureCube> TextureCube::Create(const TextureProps& props, const std::filesystem::path (&paths)[6])
  {
    auto textureRef = CreateRef<TextureCube>(props, paths);
    return textureRef;
  }

  void TextureCube::CreateFromFile(const TextureProps& props, const std::filesystem::path (&paths)[6])
  {
    for (int i = 0; i < 6; i++)
    {
      auto& path = paths[i];
      if (!std::filesystem::exists(path))
      {
        std::cerr << "Texture file not found: " << path << '\n';
        return;
      }

      m_ImageData[i] = TextureImporter::ImportFileToBuffer(path, m_TextureProps.Format, m_TextureProps.Width, m_TextureProps.Height);
    }
    Invalidate();
  }

  void TextureCube::Invalidate()
  {
    if (!RenderContext::IsReady())
    {
      return;
    }

    if (m_TextureBuffer)
    {
      for (const auto& view : m_ReadViews)
      {
        wgpuTextureViewRelease(view);
      }
      for (const auto& view : m_WriteViews)
      {
        wgpuTextureViewRelease(view);
      }
      m_ReadViews.clear();
      m_WriteViews.clear();
      wgpuTextureRelease(m_TextureBuffer);
      m_TextureBuffer = NULL;
    }

    uint32_t mipCount = 1;
    if (m_TextureProps.GenerateMips)
    {
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
    for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
      if (m_ImageData[faceIndex].Size > 0)
      {
        WriteTexture(m_ImageData[faceIndex].Data, m_TextureBuffer, m_TextureProps.Width, m_TextureProps.Height, 0, faceIndex, m_TextureProps.Format);
        m_ImageData[faceIndex].Release();
      }
    }

    WGPUTextureViewDescriptor arrayViewDesc;
    arrayViewDesc.label = RenderUtils::MakeLabel("MMB_View");
    arrayViewDesc.aspect = WGPUTextureAspect_All;
    arrayViewDesc.baseArrayLayer = 0;        // Start from the first array layer (face)
    arrayViewDesc.arrayLayerCount = 6;       // Cubemap has 6 faces
    arrayViewDesc.baseMipLevel = 0;          // Start from the base mip level
    arrayViewDesc.mipLevelCount = mipCount;  // Include all mip levels
#ifndef __EMSCRIPTEN__
    arrayViewDesc.usage = imageDesc.usage;  // Use the same usage as the texture
#endif
    arrayViewDesc.dimension = WGPUTextureViewDimension_Cube;  // View as a cubemap
    arrayViewDesc.format = RenderTypeUtils::ToRenderType(m_TextureProps.Format);
    arrayViewDesc.nextInChain = nullptr;

    m_ReadViews.push_back(wgpuTextureCreateView(m_TextureBuffer, &arrayViewDesc));

    for (uint32_t mipLevel = 0; mipLevel < mipCount; mipLevel++)
    {
      arrayViewDesc.dimension = WGPUTextureViewDimension_2DArray;
      arrayViewDesc.baseMipLevel = mipLevel;
      arrayViewDesc.mipLevelCount = 1;
      m_WriteViews.push_back(wgpuTextureCreateView(m_TextureBuffer, &arrayViewDesc));
    }
  }

  void WriteTexture(const void* pixelData, WGPUTexture target, uint32_t width, uint32_t height, uint32_t targetMip, uint32_t targetLayer, TextureFormat format)
  {
    if (!pixelData || !target)
    {
      RN_LOG_ERR("WriteTexture: Invalid pixel data or texture target");
      return;
    }

    if (width == 0 || height == 0)
    {
      RN_LOG_ERR("WriteTexture: Invalid dimensions (width: {}, height: {})", width, height);
      return;
    }

    auto* Renderer = Render::Get();
    if (Renderer == nullptr)
    {
      return;
    }

    Ref<RenderContext> renderContext = Renderer->GetRenderContext();

    if (renderContext == nullptr)
    {
      return;
    }

    const Ref<WGPUQueue> queue = renderContext->GetQueue();
    if (!queue)
    {
      RN_LOG_ERR("WriteTexture: Invalid render queue");
      return;
    }

    WGPUOrigin3D targetOrigin = {0, 0, targetLayer};

#ifdef __EMSCRIPTEN__
    WGPUImageCopyTexture dest = {
#else
    WGPUTexelCopyTextureInfo dest = {
#endif
      .texture = target,
      .mipLevel = targetMip,
      .origin = targetOrigin,
      .aspect = WGPUTextureAspect_All
    };

    if (TextureUtils::IsBlockCompressed(format))
    {
      // Block-compressed path: bytesPerRow is based on block columns, rowsPerImage is block rows
      uint32_t bytesPerBlock = TextureUtils::GetBytesPerBlock(format);
      if (bytesPerBlock == 0)
      {
        RN_LOG_ERR("WriteTexture: Unsupported compressed format {}", (int)format);
        return;
      }

      uint32_t blockCols = (width  + 3) / 4;
      uint32_t blockRows = (height + 3) / 4;
      uint32_t unalignedBytesPerRow = blockCols * bytesPerBlock;
      uint32_t alignedBytesPerRow   = (unalignedBytesPerRow + 255) & ~255u;

#ifdef __EMSCRIPTEN__
      WGPUTextureDataLayout textureLayout = {
#else
      WGPUTexelCopyBufferLayout textureLayout = {
#endif
        .offset       = 0,
        .bytesPerRow  = alignedBytesPerRow,
        .rowsPerImage = blockRows
      };

      WGPUExtent3D textureSize = {
          .width             = (width  + 3) & ~3u,
          .height            = (height + 3) & ~3u,
          .depthOrArrayLayers = 1};

      if (unalignedBytesPerRow != alignedBytesPerRow)
      {
        size_t alignedDataSize = alignedBytesPerRow * blockRows;
        std::vector<uint8_t> alignedData(alignedDataSize, 0);
        const uint8_t* srcData = static_cast<const uint8_t*>(pixelData);
        for (uint32_t row = 0; row < blockRows; row++)
        {
          memcpy(alignedData.data() + row * alignedBytesPerRow,
                 srcData            + row * unalignedBytesPerRow,
                 unalignedBytesPerRow);
        }
        wgpuQueueWriteTexture(*queue, &dest, alignedData.data(), alignedDataSize, &textureLayout, &textureSize);
      }
      else
      {
        size_t dataSize = alignedBytesPerRow * blockRows;
        wgpuQueueWriteTexture(*queue, &dest, pixelData, dataSize, &textureLayout, &textureSize);
      }
    }
    else
    {
      // Uncompressed path
      uint32_t bytesPerPixel = TextureUtils::GetBytesPerPixel(format);
      if (bytesPerPixel == 0)
      {
        RN_LOG_ERR("WriteTexture: Unsupported format {}", (int)format);
        return;
      }

      uint32_t unalignedBytesPerRow = bytesPerPixel * width;
      uint32_t alignedBytesPerRow   = (unalignedBytesPerRow + 255) & ~255u;

#ifdef __EMSCRIPTEN__
      WGPUTextureDataLayout textureLayout = {
#else
      WGPUTexelCopyBufferLayout textureLayout = {
#endif
        .offset       = 0,
        .bytesPerRow  = alignedBytesPerRow,
        .rowsPerImage = height
      };

      WGPUExtent3D textureSize = {
          .width             = width,
          .height            = height,
          .depthOrArrayLayers = 1};

      if (unalignedBytesPerRow != alignedBytesPerRow)
      {
        size_t alignedDataSize = alignedBytesPerRow * height;
        std::vector<uint8_t> alignedData(alignedDataSize, 0);
        const uint8_t* srcData = static_cast<const uint8_t*>(pixelData);
        for (uint32_t y = 0; y < height; y++)
        {
          memcpy(alignedData.data() + y * alignedBytesPerRow,
                 srcData            + y * unalignedBytesPerRow,
                 unalignedBytesPerRow);
        }
        wgpuQueueWriteTexture(*queue, &dest, alignedData.data(), alignedDataSize, &textureLayout, &textureSize);
      }
      else
      {
        size_t dataSize = alignedBytesPerRow * height;
        wgpuQueueWriteTexture(*queue, &dest, pixelData, dataSize, &textureLayout, &textureSize);
      }
    }
  }
}  // namespace WebEngine
