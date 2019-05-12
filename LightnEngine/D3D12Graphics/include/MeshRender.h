#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"
#include "SharedMaterial.h"

//マテリアルの描画範囲定義
struct MaterialDrawRange {
	MaterialDrawRange() :indexCount(0), indexOffset(0) {}
	MaterialDrawRange(uint32 indexCount, uint32 indexOffset) :indexCount(indexCount), indexOffset(indexOffset) {}

	uint32 indexCount;
	uint32 indexOffset;
};

//マテリアルごとのインデックス範囲データ
struct MaterialSlot {
	MaterialSlot(const MaterialDrawRange& range, const RefSharedMaterial& material) :range(range), material(material) {}

	const RefSharedMaterial material;
	const MaterialDrawRange range;
};

//RCG...RenderCommandGroup
//可変長のマテリアルをそれぞれ持ったインスタンスをメモリ上に連続して配置するために
//レンダーコマンドグループ＋マテリアルコマンドサイズｘマテリアル数(sizeof(StaticSingleMeshRCG) + sizeof(MaterialSlot) * materialCount)
//のメモリを独自に確保してリニアアロケーターにマップして利用する。
class StaticSingleMeshRCG {
public:
	StaticSingleMeshRCG(
		const RefVertexBufferView& _vertexBufferView,
		const RefIndexBufferView& _indexBufferView,
		const VectorArray<MaterialSlot>& materialSlots);

	~StaticSingleMeshRCG() {}

	void setupRenderCommand(RenderSettings& settings) const;
	void updateWorldMatrix(const Matrix4& worldMatrix);

	//このレンダーコマンドのマテリアルの最初のポインタを取得
	//reinterpret_castもエラーで怒られるのC-Styleキャストを使用
	constexpr RefPtr<MaterialSlot> getFirstMatrialPtr() const {
		return (MaterialSlot*)((byte*)this + sizeof(StaticSingleMeshRCG));
	}

	//このインスタンスのマテリアル数を含めたメモリサイズを取得する
	constexpr size_t getRequireMemorySize() const {
		return getRequireMemorySize(_materialSlotSize);
	}

	//マテリアル格納分も含めたトータルサイズを取得する。
	//このサイズをもとにメモリ確保しないと確実に死亡する
	static constexpr size_t getRequireMemorySize(size_t materialCount) {
		return sizeof(StaticSingleMeshRCG) + sizeof(MaterialSlot) * materialCount;
	}

private:
	Matrix4 _worldMatrix;
	const RefVertexBufferView _vertexBufferView;
	const RefIndexBufferView _indexBufferView;
	const size_t _materialSlotSize;

	//このインスタンスの後ろ(sizeof(StaticSingleMeshRCG))にマテリアルのデータを配置！！！
};

//頂点バッファとインデックスバッファのリソース管理
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
};