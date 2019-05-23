#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"
#include "SharedMaterial.h"
#include "Camera.h"
#include "AABB.h"

#define ENABLE_AABB_DEBUG_DRAW

struct VertexAndIndexBuffer;

struct IndirectCommand{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	D3D12_VERTEX_BUFFER_VIEW perInstanceVertexBufferView;
	uint32 textureIndices[4];
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct InIndirectCommand {
	uint32 meshIndex[4];
	IndirectCommand indirectCommand;
};

struct PerInstanceMeshInfo {
	Matrix4 mtxWorld;
	AABB boundingBox;
	Color color;
	uint32 indirectArgumentIndex;
};

struct InstacingVertexData {
	Matrix4 mtxWorld;
	Color color;
};

struct TextureIndex {
	uint32 t1;
	uint32 t2;
	uint32 t3;
	uint32 t4;
};

struct GpuCullingCameraConstant {
	Vector4 cameraPosition;
	Vector4 frustumPlanes[4];
};

struct CameraConstantRaw {
	Matrix4 mtxView;
	Matrix4 mtxProj;
	Vector3 cameraPosition;
};

struct PerMeshData {
	VectorArray<Matrix4> matrices;
	VectorArray<TextureIndex> textureIndices;
};

struct InitSettingsPerStaticMultiMesh {
	VectorArray<String> meshNames;
	VectorArray<PerMeshData> meshes;
	VectorArray<String> textureNames;
};

class StaticMultiMeshRCG {
public:
	//�J�����O�Ώۂ̍s��f�[�^�ƕ`�����n���ď�����
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const InitSettingsPerStaticMultiMesh& initInfo);

	//GPU�J�����O�Ŏg�p����J���������X�V
	void updateCullingCameraInfo(const Camera& camera, uint32 frameIndex);

	//ComputeShader��GPU�J�����O�����s
	void onCompute(RefPtr<CommandContext> commandContext, uint32 frameIndex);

	//GPU�J�����O��̏��ŕ`��
	void setupRenderCommand(RenderSettings& settings);

	//�j��
	void destroy();

	ConstantBufferMaterial cb;

	RootSignature rootSignature;
	PipelineState pipelineState;

	BufferView srv;

private:
	//GPU�J�����O�̌��ʂ��i�[����o�b�t�@�̃��\�[�X�o���A��ݒ�
	void culledBufferBarrier(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, uint32 frameIndex);

private:

	UINT _indirectArgumentCount;
	UINT _meshCount;
	UINT _indirectArgumentDstCounterOffset;
	UINT _gpuCullingDispatchCount;

	ConstantBufferMaterial _gpuCullingCameraConstantBuffers;
	VectorArray<uint32> _uavCounterOffsets;

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

#ifdef ENABLE_AABB_DEBUG_DRAW
	VectorArray<AABB> _boundingBoxies;
#endif
};