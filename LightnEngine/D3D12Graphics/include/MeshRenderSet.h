#pragma once

#include <Utility.h>
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

	void setupRenderCommand(RenderSettings& settings) const;
	RefPtr<SharedMaterial> getMaterial(uint32 index);

//private://CreateŠÖ”‚ğì‚Á‚½‚çPrivate‚É‚·‚éBB
	UniquePtr<VertexBuffer> _vertexBuffer;
	UniquePtr<IndexBuffer> _indexBuffer;
	VectorArray<MaterialSlot> _materialSlots;
};