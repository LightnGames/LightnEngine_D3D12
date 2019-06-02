#pragma once
#include "Utility.h"
#include "stdafx.h"
#include "SharedMaterial.h"
#include "BufferView.h"

#include "AABB.h"
#include "CommandContext.h"
#include "Camera.h"
#include "StaticMultiMesh.h"

class StaticSingleMesh {
public:
	void create(RefPtr<ID3D12Device> device, const String& meshName,const ConstantBufferFrame& cameraBuffer, const VectorArray<InitSettingsPerSingleMesh>& materialInfos);
	void setupRenderCommand(RenderSettings& settings);

	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<MaterialCommandGraphics> _materials;
	RefPtr<VertexAndIndexBuffer> _mesh;
};

class StaticMultiMesh {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const ConstantBufferFrame& cameraBuffer, const InitSettingsPerStaticMultiMesh& initInfo);
	void onCompute(RefPtr<CommandContext> commandContext, uint32 frameIndex);

	void setupCommand(RenderSettings& settings);

	void updateCullingCameraInfo(const Camera& camera, uint32 frameIndex);

	//GPUカリングの結果を格納するバッファのリソースバリアを設定
	void culledBufferBarrier(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, uint32 frameIndex) const;


	UINT _indirectArgumentCount;
	UINT _meshCount;
	UINT _indirectArgumentDstCounterOffset;
	UINT _gpuCullingDispatchCount;
	VectorArray<uint32> _uavCounterOffsets;

	CommandSignature _commandSignature;

	VectorArray<RefPtr<GpuBuffer>> _gpuDrivenInstanceCulledBuffer;
	MaterialCommandCompute _gpuCullingCommand;
	MaterialCommandCompute _setupIndirectArgumentCommand;
	MaterialCommandGraphics _drawCommand;

	RefPtr<ConstantBuffer> _gpuCullingCameraConstantBuffers[FrameCount];
	RefPtr<GpuBuffer> _indirectArgumentDstBuffer;
	RefPtr<GpuBuffer> _uavCounterReset;

	VectorArray<AABB> _boundingBoxies;
};