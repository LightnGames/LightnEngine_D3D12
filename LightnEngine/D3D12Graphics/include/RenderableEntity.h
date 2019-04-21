#pragma once

#include <LMath.h>
#include <Utility.h>
#include "stdafx.h"
#include "GpuResource.h"

struct RenderSettings;
class MeshRenderSet;
class SharedMaterial;

struct IRenderableEntity {
	virtual ~IRenderableEntity() {}
	virtual	void setupRenderCommand(RenderSettings& settings) const = 0;
};

class StaticSingleMeshRender :public IRenderableEntity {
public:
	~StaticSingleMeshRender(){}
	StaticSingleMeshRender(RefPtr<MeshRenderSet> renderSet);

	void setupRenderCommand(RenderSettings& settings) const override;
	void updateWorldMatrix(const Matrix4& worldMatrix);

	void setMaterial(uint32 index, RefPtr<SharedMaterial> material);
	RefPtr<SharedMaterial> getMaterial(uint32 index);

	RefPtr<MeshRenderSet> _renderSet;
	UnorderedMap<uint32, RefPtr<SharedMaterial>> _materialOverrides;

	//RefVertexBufferView _vertexBufferView;
	//RefIndexBufferView _indexBufferView;
private:

	Matrix4 _worldMatrix;
};