#pragma once

#include <Utility.h>
struct RenderSettings;
class SharedMaterial;
class VertexBuffer;
class IndexBuffer;

//�}�e���A�����Ƃ̃C���f�b�N�X�͈̓f�[�^
struct MaterialSlot {
	uint32 indexCount;
	uint32 indexOffset;
	RefPtr<SharedMaterial> material;
};

class MeshRenderSet {
public:
	MeshRenderSet(
		UniquePtr<VertexBuffer> vertexBuffer, 
		UniquePtr<IndexBuffer> indexBuffer, 
		const VectorArray<MaterialSlot>& materialSlots);

	~MeshRenderSet() {}

	void setupRenderCommand(RenderSettings& settings) const;

	//�C���f�b�N�X�ԍ��̃X���b�g�ɐV�����}�e���A�����Z�b�g����
	void setMaterial(uint32 index, RefPtr<SharedMaterial> material);
	RefPtr<SharedMaterial> getMaterial(uint32 index);
	RefPtr<MaterialSlot> getMaterialSlot(uint32 index);

private:
	UniquePtr<VertexBuffer> _vertexBuffer;
	UniquePtr<IndexBuffer> _indexBuffer;
	VectorArray<MaterialSlot> _materialSlots;
};