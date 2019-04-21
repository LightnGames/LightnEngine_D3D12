#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"

struct RenderSettings;
class SharedMaterial;

//マテリアルごとのインデックス範囲データ
struct MaterialSlot {
	MaterialSlot() :indexCount(0), indexOffset(0), material(nullptr) {}
	uint32 indexCount;
	uint32 indexOffset;
	RefPtr<SharedMaterial> material;
};

struct IRenderableEntity {
	virtual ~IRenderableEntity() {}
	virtual	void setupRenderCommand(RenderSettings& settings) = 0;
};

class StaticSingleMeshRender{
public:
	StaticSingleMeshRender(
		const RefVertexBufferView& _vertexBufferView,
		const RefIndexBufferView& _indexBufferView,
		const VectorArray<MaterialSlot>& materialSlots);
	
	~StaticSingleMeshRender(){}

	void setupRenderCommand(RenderSettings& settings) const;
	void updateWorldMatrix(const Matrix4& worldMatrix);

	void setMaterial(uint32 index, RefPtr<SharedMaterial> material);
	RefPtr<SharedMaterial> getMaterial(uint32 index) const;

private:
	const RefVertexBufferView _vertexBufferView;
	const RefIndexBufferView _indexBufferView;
	VectorArray<MaterialSlot> _materialSlots;
	Matrix4 _worldMatrix;
};

struct VertexAndIndexBuffer {
	VertexAndIndexBuffer(const VectorArray<MaterialSlot>& materialSlots) :materialSlots(materialSlots) {}

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	VectorArray<MaterialSlot> materialSlots;
};