#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"
#include "SharedMaterial.h"
#include "Camera.h"
#include "AABB.h"

//�}�e���A���̕`��͈͒�`
struct MaterialDrawRange {
	MaterialDrawRange() :indexCount(0), indexOffset(0) {}
	MaterialDrawRange(uint32 indexCount, uint32 indexOffset) :indexCount(indexCount), indexOffset(indexOffset) {}

	uint32 indexCount;
	uint32 indexOffset;
};

//�}�e���A�����Ƃ̃C���f�b�N�X�͈̓f�[�^
struct MaterialSlot {
	MaterialSlot(const MaterialDrawRange& range, const RefSharedMaterial& material) :range(range), material(material) {}

	const RefSharedMaterial material;
	const MaterialDrawRange range;
};

//RCG...RenderCommandGroup
//�ϒ��̃}�e���A�������ꂼ�ꎝ�����C���X�^���X����������ɘA�����Ĕz�u���邽�߂�
//�����_�[�R�}���h�O���[�v�{�}�e���A���R�}���h�T�C�Y���}�e���A����(sizeof(StaticSingleMeshRCG) + sizeof(MaterialSlot) * materialCount)
//�̃�������Ǝ��Ɋm�ۂ��ă��j�A�A���P�[�^�[�Ƀ}�b�v���ė��p����B
class StaticSingleMeshRCG {
public:
	StaticSingleMeshRCG(
		const RefVertexBufferView& _vertexBufferView,
		const RefIndexBufferView& _indexBufferView,
		const VectorArray<MaterialSlot>& materialSlots);

	~StaticSingleMeshRCG() {}

	void setupRenderCommand(RenderSettings& settings) const;
	void updateWorldMatrix(const Matrix4& worldMatrix);

	//���̃����_�[�R�}���h�̃}�e���A���̍ŏ��̃|�C���^���擾
	//reinterpret_cast���G���[�œ{�����C-Style�L���X�g���g�p
	constexpr RefPtr<MaterialSlot> getFirstMatrialPtr() const {
		return (MaterialSlot*)((byte*)this + sizeof(StaticSingleMeshRCG));
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

	//���̃C���X�^���X�̌��(sizeof(StaticSingleMeshRCG))�Ƀ}�e���A���̃f�[�^��z�u�I�I�I
};

struct RefVertexAndIndexBuffer {
	RefVertexAndIndexBuffer(const RefVertexBufferView& vertexView,
		const RefIndexBufferView& indexView,
		const MaterialDrawRange& drawRange) :
		vertexView(vertexView), indexView(indexView), drawRange(drawRange) {
	}

	const RefVertexBufferView vertexView;
	const RefIndexBufferView indexView;
	const MaterialDrawRange drawRange;
};

//���_�o�b�t�@�ƃC���f�b�N�X�o�b�t�@�̃��\�[�X�Ǘ�
struct VertexAndIndexBuffer {
	VertexAndIndexBuffer(const VectorArray<MaterialDrawRange>& materialDrawRanges) :materialDrawRanges(materialDrawRanges) {}

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	AABB boundingBox;
	VectorArray<MaterialDrawRange> materialDrawRanges;
};