#include "Sampler.h"
#include "render/RenderContext.h"
#include "render/RenderUtils.h"

Ref<Sampler> Sampler::Create(SamplerProps props) {
  WGPUSamplerDescriptor samplerDesc = {};

  if (!props.Name.empty()) {
    samplerDesc.label = props.Name.c_str();
  }

	samplerDesc.addressModeU = RenderTypeUtils::ToRenderType(props.WrapFormat);
	samplerDesc.addressModeV = RenderTypeUtils::ToRenderType(props.WrapFormat);
	samplerDesc.addressModeW = RenderTypeUtils::ToRenderType(props.WrapFormat);

  switch (props.MagFilterFormat) {
    case Nearest:
      samplerDesc.magFilter = WGPUFilterMode_Nearest;
      break;
    case Linear:
      samplerDesc.magFilter = WGPUFilterMode_Linear;
      break;
  }

  switch (props.MinFilterFormat) {
    case Nearest:
      samplerDesc.minFilter = WGPUFilterMode_Nearest;
      break;
    case Linear:
      samplerDesc.minFilter = WGPUFilterMode_Linear;
      break;
  }

  switch (props.MipFilterFormat) {
    case Nearest:
      samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
      break;
    case Linear:
      samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
      break;
  }

  samplerDesc.lodMinClamp = props.LodMinClamp;
  samplerDesc.lodMaxClamp = props.LodMaxClamp;

  switch (props.Compare) {
    case CompareUndefined:
      samplerDesc.compare = WGPUCompareFunction_Undefined;
      break;
    case Less:
      samplerDesc.compare = WGPUCompareFunction_Less;
      break;
  }
	samplerDesc.maxAnisotropy = props.Ans;

  auto nativeSampler = CreateRef<WGPUSampler>(wgpuDeviceCreateSampler(RenderContext::GetDevice(), &samplerDesc));
  auto sampler = CreateRef<Sampler>(nativeSampler);

  return sampler;
}

void Sampler::Release() {
	wgpuSamplerRelease(*m_Sampler); // TODO better do
}
