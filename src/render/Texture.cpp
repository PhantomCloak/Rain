#include "Texture.h"
#include "render/Render.h"
#include "render/RenderContext.h"

Ref<Texture> Texture::Create(TextureProps props) {
  WGPUTextureDescriptor textureDesc = {
      .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
      .dimension = WGPUTextureDimension_2D,
      .size = {static_cast<uint32_t>(props.Dimensions.x), static_cast<uint32_t>(props.Dimensions.y), 1},
      .format = props.Format == TextureFormat::RGBA ? WGPUTextureFormat_BGRA8Unorm : WGPUTextureFormat_Depth24Plus,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };

  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();

  WGPUTexture texture = wgpuDeviceCreateTexture(renderContext->GetDevice(), &textureDesc);
  WGPUTextureView textureView = wgpuTextureCreateView(texture, nullptr);

  auto textureRef = CreateRef<Texture>();

  textureRef->Buffer = texture;
  textureRef->View = textureView;

  return textureRef;
}
