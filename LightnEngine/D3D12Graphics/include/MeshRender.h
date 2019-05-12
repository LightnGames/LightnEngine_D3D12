#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"
#include "SharedMaterial.h"

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

//���_�o�b�t�@�ƃC���f�b�N�X�o�b�t�@�̃��\�[�X�Ǘ�
struct VertexAndIndexBuffer {
	VertexAndIndexBuffer(const VectorArray<MaterialDrawRange>& materialDrawRanges) :materialDrawRanges(materialDrawRanges) {}

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	VectorArray<MaterialDrawRange> materialDrawRanges;
};

struct IndirectCommand
{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	D3D12_VERTEX_BUFFER_VIEW perInstanceVertexBufferView;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct ObjectInfo {
	Matrix4 mtxWorld;
	Vector3 startPosAABB;
	Vector3 endposAABB;
	Color color;
	uint32 indirectArgumentIndex;
};

struct PerInstanceVertex {
	Matrix4 mtxWorld;
	Color color;
};

struct PerInstanceIndirectArgument {
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	uint32 indexCount;
	uint32 counterOffset;
	uint32 instanceCount;
};

struct SceneConstant {
	Vector4 cameraPosition;
	Vector4 frustumPlanes[4];
};

struct IndirectMeshInfo {
	uint32 maxInstanceCount;
	RefPtr<VertexAndIndexBuffer> vertexAndIndexBuffer;
	VectorArray<ObjectInfo> matrices;
};

class StaticMultiMeshRCG {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const VectorArray<IndirectMeshInfo>& meshes);
	void onCompute(RefPtr<CommandContext> commandContext, uint32 frameIndex);
	void setupRenderCommand(RenderSettings& settings);
	void updateCullingCameraInfo(const SceneConstant& constant, uint32 frameIndex);
	void destroy();

private:
	void culledBufferBarrier(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, uint32 frameIndex);

private:

	UINT _indirectArgumentCount;
	UINT _indirectArgumentDstCounterOffset;
	UINT _gpuCullingDispatchCount;

	ConstantBufferMaterial gpuCullingCameraInfo;
	VectorArray<PerInstanceIndirectArgument> _indirectMeshes;

	CommandSignature _commandSignature;
	RootSignature _cullingComputeRootSignature;
	PipelineState _cullingComputeState;
	RootSignature _setupCommandComputeRootSignature;
	PipelineState _setupCommandComputeState;

	GpuBuffer* _gpuDrivenInstanceCulledBuffer;//GPU�J�����O��̕`��Ώۂ̃C���X�^���X�̃��[���h�s��
	GpuBuffer _gpuDrivenInstanceMatrixBuffer;//�J�����O�O�̃V�[���ɔz�u����Ă���C���X�^���X�̃��[���h�s��
	GpuBuffer _indirectArgumentDstBuffer[FrameCount];//GPU�J�����O���ExecuteIndirect�ɓn�����`�����
	GpuBuffer _indirectArgumentSourceBuffer[FrameCount];//�J�����O�O�̃V�[���ɔz�u����Ă���C���X�^���X�̕`�����
	GpuBuffer _indirectArgumentOffsetsBuffer;
	GpuBuffer _uavCounterReset;//UAV��AppendStructuredBuffer�̃J�E���g�������O�ɖ߂����߂�UINT�P�̃o�b�t�@

	BufferView _setupCommandUavView[FrameCount];
	BufferView _gpuDriventInstanceCulledSRV[FrameCount];//GPU�J�����O��̏���ǂݍ��ރo�b�t�@��SRV
	BufferView _gpuDriventInstanceCulledUAV[FrameCount];//GPU�J�����O��̏����������ރo�b�t�@��UAV
	BufferView _gpuDrivenInstanceMatrixView;
};