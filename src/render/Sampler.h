#pragma once

#include "core/Ref.h"
#include "webgpu/webgpu.h"
#include <string>

enum TextureWrappingFormat {
  ClampToEdges,
  Repeat
};

enum FilterMode {
  Nearest,
  Linear
};

enum CompareMode {
  CompareUndefined,
  Less
};

struct SamplerProps {
	std::string Name;
  TextureWrappingFormat WrapFormat;
  FilterMode MagFilterFormat;
  FilterMode MinFilterFormat;
  FilterMode MipFilterFormat;
  CompareMode Compare;
  float LodMinClamp = 0.0f;
  float LodMaxClamp = 10.0f;
};

class Sampler {
 public:
  Ref<WGPUSampler> GetNativeSampler() { return m_Sampler; }
  static Ref<Sampler> Create(SamplerProps props);
	static SamplerProps GetDefaultProps(std::string name = "", float lodMinClamp = 0.0f, float lodMaxClamp = 80.0f) {
    return SamplerProps{
				.Name = name,
        .WrapFormat = TextureWrappingFormat::Repeat,
        .MagFilterFormat = FilterMode::Linear,
        .MinFilterFormat = FilterMode::Linear,
				.MipFilterFormat = FilterMode::Linear,
        .Compare = CompareMode::CompareUndefined,
        .LodMinClamp = lodMinClamp,
        .LodMaxClamp = lodMaxClamp};
  }

  Sampler(Ref<WGPUSampler> sampler)
      : m_Sampler(sampler) {};
 private:
  Ref<WGPUSampler> m_Sampler;
};
