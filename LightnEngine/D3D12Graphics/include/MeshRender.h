#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"
#include "SharedMaterial.h"
#include "Camera.h"
#include "AABB.h"

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

//頂点バッファとインデックスバッファのリソース管理
struct VertexAndIndexBuffer {
	VertexAndIndexBuffer(const VectorArray<MaterialDrawRange>& materialDrawRanges) :materialDrawRanges(materialDrawRanges) {}

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	AABB boundingBox;
	VectorArray<MaterialDrawRange> materialDrawRanges;
};