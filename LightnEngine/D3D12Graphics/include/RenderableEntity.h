#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"
#include "SharedMaterial.h"

//マテリアルの描画範囲定義
struct MaterialDrawRange {
	MaterialDrawRange(uint32 indexCount, uint32 indexOffset) :indexCount(indexCount), indexOffset(indexOffset) {}

	uint32 indexCount;
	uint32 indexOffset;
};

//マテリアルごとのインデックス範囲データ
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

private:
	Matrix4 _worldMatrix;
	const RefVertexBufferView _vertexBufferView;
	const RefIndexBufferView _indexBufferView;
	VectorArray<MaterialSlot> _materialSlots;
};

class StaticSingleMeshRender {
public:
	RefPtr<SharedMaterial> getMaterial(uint32 index) const;
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<RefPtr<SharedMaterial>> _materials;
	RefPtr<StaticSingleMeshRCG> _rcg;
};

//頂点バッファとインデックスバッファのリソース管理
struct VertexAndIndexBuffer {
	VertexAndIndexBuffer(const VectorArray<MaterialDrawRange>& materialDrawRanges) :materialDrawRanges(materialDrawRanges) {}

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	VectorArray<MaterialDrawRange> materialDrawRanges;
};