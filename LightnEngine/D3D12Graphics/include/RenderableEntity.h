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

	const RefSharedMaterial material;
	const MaterialDrawRange range;
};

//RCG...RenderCommandGroup
//可変長のマテリアルをそれぞれ持ったインスタンスをメモリ上に連続して配置するために
//レンダーコマンドグループ＋マテリアルコマンドサイズｘマテリアル数(sizeof(StaticSingleMeshRCG) + sizeof(MaterialSlot) * materialCount)
//のメモリを独自に確保してリニアアロケーターにマップして利用する。
class StaticSingleMeshRCG{
public:
	StaticSingleMeshRCG(
		const RefVertexBufferView& _vertexBufferView,
		const RefIndexBufferView& _indexBufferView,
		const VectorArray<MaterialSlot>& materialSlots);
	
	~StaticSingleMeshRCG(){}

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

class StaticSingleMeshRender {
public:
	StaticSingleMeshRender();
	StaticSingleMeshRender(const VectorArray<RefPtr<SharedMaterial>>& materials, RefPtr<StaticSingleMeshRCG> rcg);
	
	//マテリアルをインデックスで取得
	RefPtr<SharedMaterial> getMaterial(uint32 index) const;

	//マテリアル配列を取得
	VectorArray<RefPtr<SharedMaterial>>& getMaterials();

	//このメッシュの描画用ワールド行列を更新
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