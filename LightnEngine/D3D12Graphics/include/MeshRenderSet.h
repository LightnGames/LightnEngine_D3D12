#pragma once

#include <Utility.h>
struct RenderSettings;
class SharedMaterial;
class VertexBuffer;
class IndexBuffer;

//マテリアルごとのインデックス範囲データ
struct MaterialSlot {
	uint32 indexCount;
	uint32 indexOffset;
	RefPtr<SharedMaterial> material;
};

class MeshRenderSet {
public:
	MeshRenderSet(
		UniquePtr<VertexBuffer> vertexBuffer, 
		UniquePtr<IndexBuffer> indexBuffer, 
		const VectorArray<MaterialSlot>& materialSlots);

	~MeshRenderSet() {}

	void setupRenderCommand(RenderSettings& settings) const;

	//インデックス番号のスロットに新しいマテリアルをセットする
	void setMaterial(uint32 index, RefPtr<SharedMaterial> material);
	RefPtr<SharedMaterial> getMaterial(uint32 index);
	RefPtr<MaterialSlot> getMaterialSlot(uint32 index);

private:
	UniquePtr<VertexBuffer> _vertexBuffer;
	UniquePtr<IndexBuffer> _indexBuffer;
	VectorArray<MaterialSlot> _materialSlots;
};