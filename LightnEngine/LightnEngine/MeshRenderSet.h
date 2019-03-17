#pragma once

#include "Utility.h"
struct RenderSettings;
class SharedMaterial;
class VertexBuffer;
class IndexBuffer;

struct MaterialSlot {
	uint32 indexCount;
	uint32 indexOffset;
	RefPtr<SharedMaterial> material;
};

class MeshRenderSet {
public:
	~MeshRenderSet() {}

	void setupRenderCommand(RenderSettings& settings);
	RefPtr<SharedMaterial> getMaterial(uint32 index);

	UniquePtr<VertexBuffer> vertexBuffer;
	UniquePtr<IndexBuffer> indexBuffer;
	VectorArray<MaterialSlot> materialSlots;
};