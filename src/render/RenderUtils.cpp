#include "RenderUtils.h"

void LayoutUtils::SetVisibility(WGPUBindGroupLayoutEntry& entry,
                                GroupLayoutVisibility visibility) {
  switch (visibility) {
    case GroupLayoutVisibility::Vertex:
      entry.visibility = WGPUShaderStage_Vertex;
      break;
    case GroupLayoutVisibility::Fragment:
      entry.visibility = WGPUShaderStage_Fragment;
      break;
    case GroupLayoutVisibility::Both:
      entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
      break;
  }
}

void LayoutUtils::SetType(WGPUBindGroupLayoutEntry& entry, GroupLayoutType type) {
  switch (type) {
    case GroupLayoutType::Uniform:
      entry.buffer.type = WGPUBufferBindingType_Uniform;
      break;
    case GroupLayoutType::Texture:
      entry.texture.sampleType = WGPUTextureSampleType_Float;
      entry.texture.viewDimension = WGPUTextureViewDimension_2D;
      break;
    case GroupLayoutType::TextureDepth:
      entry.texture.sampleType = WGPUTextureSampleType_Depth;
      entry.texture.viewDimension = WGPUTextureViewDimension_2D;
      break;
    case GroupLayoutType::Sampler:
      entry.sampler.type = WGPUSamplerBindingType_Filtering;
      break;
    case GroupLayoutType::SamplerCompare:
      entry.sampler.type = WGPUSamplerBindingType_Comparison;
      break;
  }
}

std::vector<WGPUBindGroupLayoutEntry> LayoutUtils::ParseGroupLayout(GroupLayout layout) {
  std::vector<WGPUBindGroupLayoutEntry> groups;
  int groupsMemberCtx = 0;

  // We may need to account for buffer.minBindingSize
  for (auto layoutItem : layout) {
    WGPUBindGroupLayoutEntry entry = {};

    entry.binding = groupsMemberCtx++;
    SetVisibility(entry, layoutItem.Visiblity);
    SetType(entry, layoutItem.Type);

    groups.push_back(entry);
  }

  return groups;
}

WGPUBindGroupLayout LayoutUtils::CreateBindGroup(std::string label, WGPUDevice device, GroupLayout layout) {
	std::vector<WGPUBindGroupLayoutEntry> entries = ParseGroupLayout(layout);

	// TODO(memory): We need to introduce custom allocator in the future
	// or alternatively use callbacks to handle release
	WGPUBindGroupLayoutDescriptor* descriptor = new WGPUBindGroupLayoutDescriptor{};
	descriptor->label = label.c_str();
	descriptor->entries = entries.data();
	descriptor->entryCount = entries.size();

	return wgpuDeviceCreateBindGroupLayout(device, descriptor);
}
