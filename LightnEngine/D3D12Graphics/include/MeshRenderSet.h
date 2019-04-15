#pragma once

#include <Utility.h>
#include <LMath.h>
#include <GpuResource.h>

struct RenderSettings;
class SharedMaterial;

//�}�e���A�����Ƃ̃C���f�b�N�X�͈̓f�[�^
struct MaterialSlot {
	uint32 indexCount;
	uint32 indexOffset;
	RefPtr<SharedMaterial> material;
};

class MeshRenderSet {
public:
	MeshRenderSet(const VectorArray<MaterialSlot>& materialSlots);

	~MeshRenderSet();

	void setupRenderCommand(RenderSettings& settings) const;

	//�C���f�b�N�X�ԍ��̃X���b�g�ɐV�����}�e���A�����Z�b�g����
	void setMaterial(uint32 index, RefPtr<SharedMaterial> material);
	RefPtr<SharedMaterial> getMaterial(uint32 index) const;

	VertexBuffer _vertexBuffer;
	IndexBuffer _indexBuffer;

private:
	VectorArray<MaterialSlot> _materialSlots;
};