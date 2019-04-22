#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"
#include "SharedMaterial.h"

//�}�e���A���̕`��͈͒�`
struct MaterialDrawRange {
	MaterialDrawRange(uint32 indexCount, uint32 indexOffset) :indexCount(indexCount), indexOffset(indexOffset) {}

	uint32 indexCount;
	uint32 indexOffset;
};

//�}�e���A�����Ƃ̃C���f�b�N�X�͈̓f�[�^
struct MaterialSlot {
	MaterialSlot(const MaterialDrawRange& range, const RefSharedMaterial& material) :range(range), material(material) {}

	const MaterialDrawRange range;
	const RefSharedMaterial material;
};

struct IRenderableEntity {
	virtual ~IRenderableEntity() {}
	virtual	void setupRenderCommand(RenderSettings& settings) = 0;
};

//RCG...RenderCommandGroup
class StaticSingleMeshRCG{
public:
	StaticSingleMeshRCG(
		const RefVertexBufferView& _vertexBufferView,
		const RefIndexBufferView& _indexBufferView,
		const VectorArray<MaterialSlot>& materialSlots);
	
	~StaticSingleMeshRCG(){}

	void setupRenderCommand(RenderSettings& settings) const;
	void updateWorldMatrix(const Matrix4& worldMatrix);

	//���̃����_�[�R�}���h�̃}�e���A���̍ŏ��̃|�C���^���擾
	//reinterpret_cast���G���[�œ{�����C-Style�L���X�g���g�p
	constexpr RefPtr<MaterialSlot> getFirstMatrialPtr() const {
		return (MaterialSlot*)(this + sizeof(StaticSingleMeshRCG));
	}

	//���̃C���X�^���X�̃}�e���A�������܂߂��������T�C�Y���擾����
	constexpr size_t getRequireMemorySize() const {
		return getRequireMemorySize(_materialSlotSize);
	}

	//�}�e���A���i�[�����܂߂��g�[�^���T�C�Y���擾����B
	//���̃T�C�Y�����ƂɃ������m�ۂ��Ȃ��Ɗm���Ɏ��S����
	static constexpr size_t getRequireMemorySize(size_t materialCount) {
		return sizeof(StaticSingleMeshRCG) + sizeof(MaterialSlot) * materialCount;
	}

private:
	Matrix4 _worldMatrix;
	const RefVertexBufferView _vertexBufferView;
	const RefIndexBufferView _indexBufferView;
	const size_t _materialSlotSize;
	//VectorArray<MaterialSlot> _materialSlots;

	//���̃C���X�^���X�̌��(sizeof(StaticSingleMeshRCG))�Ƀ}�e���A���̃f�[�^��z�u�I�I�I
};

class StaticSingleMeshRender {
public:
	RefPtr<SharedMaterial> getMaterial(uint32 index) const;
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<RefPtr<SharedMaterial>> _materials;
	RefPtr<StaticSingleMeshRCG> _rcg;
};

//���_�o�b�t�@�ƃC���f�b�N�X�o�b�t�@�̃��\�[�X�Ǘ�
struct VertexAndIndexBuffer {
	VertexAndIndexBuffer(const VectorArray<MaterialDrawRange>& materialDrawRanges) :materialDrawRanges(materialDrawRanges) {}

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	VectorArray<MaterialDrawRange> materialDrawRanges;
};