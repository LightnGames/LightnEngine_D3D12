#pragma once
#include "Utility.h"
#include "stdafx.h"
#include "SharedMaterial.h"
#include "BufferView.h"

#include "AABB.h"
#include "Camera.h"

#define ENABLE_AABB_DEBUG_DRAW

class StaticSingleMesh {
public:
	void create(RefPtr<ID3D12Device> device, const String& meshName, const ConstantBufferFrame& cameraBuffer, const VectorArray<InitSettingsPerSingleMesh>& materialInfos);
	
	//�e�����_�����O�p�X
	void setupDepthPassCommand(RenderSettings& settings);
	void setupMainPassCommand(RenderSettings& settings);

	//�e�C���X�^���X���Ƃ̃��[���h�s����X�V
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<MaterialCommandGraphics> _depthMaterials;
	VectorArray<MaterialCommandGraphics> _mainMaterials;
	RefPtr<VertexAndIndexBuffer> _mesh;
};

struct PerInstanceMeshInfo {
	Matrix4 mtxWorld;
	AABB boundingBox;
	uint32 indirectArgumentIndex;
};

struct InstacingVertexData {
	Matrix4 mtxWorld;
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

struct IndirectCommand {
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	D3D12_VERTEX_BUFFER_VIEW perInstanceVertexBufferView;
	TextureIndex textureIndices;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct InIndirectCommand {
	uint32 meshIndex[4];
	IndirectCommand indirectCommand;
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

struct InitBufferInfo {
	ConstantBufferFrame& cameraBuffer;
	ConstantBufferFrame& directionalLightBuffer;
	ConstantBufferFrame& pointLightBuffer;
};

class StaticMultiMesh {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const InitBufferInfo& bufferInfo, const InitSettingsPerStaticMultiMesh& initInfo);
	
	//GPU�J�����O
	void onCompute(RenderSettings& settings);

	//�e�����_�����O�p�X
	void setupDepthPassCommand(RenderSettings& settings);
	void setupMainPassCommand(RenderSettings& settings);

	//�V�F�[�_�[�ɓn�����߂̎���������X�V
	void updateCullingCameraInfo(const Camera& camera, uint32 frameIndex);

	//GPU�J�����O�̌��ʂ��i�[����o�b�t�@�̃��\�[�X�o���A��ݒ�
	void culledBufferBarrier(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, uint32 frameIndex) const;

	UINT _indirectArgumentCount;
	UINT _meshCount;
	UINT _indirectArgumentDstCounterOffset;
	UINT _gpuCullingDispatchCount;
	VectorArray<uint32> _uavCounterOffsets;

	CommandSignature _depthPassCommandSignature;
	CommandSignature _mainPassCommandSignature;
	MaterialCommandCompute _gpuCullingCommand;
	MaterialCommandCompute _setupIndirectArgumentCommand;
	MaterialCommandGraphics _depthPassCommand;
	MaterialCommandGraphics _mainPassCommand;

	VectorArray<RefPtr<GpuBuffer>> _gpuDrivenInstanceCulledBuffers;
	RefPtr<ConstantBuffer> _gpuCullingCameraConstantBuffers[FrameCount];
	RefPtr<GpuBuffer> _indirectArgumentDstBuffer;
	RefPtr<GpuBuffer> _uavCounterReset;

#ifdef ENABLE_AABB_DEBUG_DRAW
	VectorArray<AABB> _boundingBoxies;
#endif
};