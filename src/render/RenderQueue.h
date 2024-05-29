#pragma once
#include <map>
#include "Mesh.h"
#include "GameObject.h"

#define PRE_ALLOCATED_BATCH_GROUPS 2
#define RENDER_BATCH_SIZE 256

class RenderQueue {
	public:
		static void Init();
		static void AddQueue(GameObject* gameObject);
		static void Clear();
		static void DrawEntities(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);
	private:
		static std::vector<uint32_t> offsetArray;
};
