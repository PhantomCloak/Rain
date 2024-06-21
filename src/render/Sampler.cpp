#include "Sampler.h"
#include "render/RenderContext.h"

 Ref<Sampler> Sampler::Create(SamplerProps props) {
  WGPUSamplerDescriptor samplerDesc = {};  // Zero-initialize the struct

  // Set the properties
  samplerDesc.addressModeU = WGPUAddressMode_Repeat;
  samplerDesc.addressModeV = WGPUAddressMode_Repeat;
  samplerDesc.addressModeW = WGPUAddressMode_Repeat;
  samplerDesc.magFilter = WGPUFilterMode_Linear;
  samplerDesc.minFilter = WGPUFilterMode_Linear;
  samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
  samplerDesc.lodMinClamp = 0.0f;
  samplerDesc.lodMaxClamp = 80.0f;
  samplerDesc.compare = WGPUCompareFunction_Undefined;
  samplerDesc.maxAnisotropy = 1;

  auto nativeSampler = CreateRef<WGPUSampler>(wgpuDeviceCreateSampler(RenderContext::GetDevice(), &samplerDesc));
	auto sampler = CreateRef<Sampler>(nativeSampler);

	return sampler;
}
