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
	//カリング対象の行列データと描画情報を渡して初期化
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const InitSettingsPerStaticMultiMesh& initInfo);

	//GPUカリングで使用するカメラ情報を更新
	void updateCullingCameraInfo(const Camera& camera, uint32 frameIndex);

	//ComputeShaderでGPUカリングを実行
	void onCompute(RefPtr<CommandContext> commandContext, uint32 frameIndex);

	//GPUカリング後の情報で描画
	void setupRenderCommand(RenderSettings& settings);

	//破棄
	void destroy();

	ConstantBufferMaterial cb;

	RootSignature rootSignature;
	PipelineState pipelineState;

	BufferView srv;

private:
	//GPUカリングの結果を格納するバッファのリソースバリアを設定
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

	GpuBuffer* _gpuDrivenInstanceCulledBuffer;//GPUカリング後の描画対象のインスタンスのワールド行列
	GpuBuffer _gpuDrivenInstanceMatrixBuffer;//カリング前のシーンに配置されているインスタンスのワールド行列
	GpuBuffer _indirectArgumentDstBuffer[FrameCount];//GPUカリング後のExecuteIndirectに渡される描画引数
	GpuBuffer _indirectArgumentSourceBuffer[FrameCount];//カリング前のシーンに配置されているインスタンスの描画引数
	GpuBuffer _indirectArgumentOffsetsBuffer;
	GpuBuffer _uavCounterReset;//UAVのAppendStructuredBufferのカウント引数を０に戻すためのUINT１つのバッファ

	BufferView _setupCommandUavView[FrameCount];
	BufferView _gpuDriventInstanceCulledSRV[FrameCount];//GPUカリング後の情報を読み込むバッファのSRV
	BufferView _gpuDriventInstanceCulledUAV[FrameCount];//GPUカリング後の情報を書き込むバッファのUAV
	BufferView _gpuDrivenInstanceMatrixView;

#ifdef ENABLE_AABB_DEBUG_DRAW
	VectorArray<AABB> _boundingBoxies;
#endif
};