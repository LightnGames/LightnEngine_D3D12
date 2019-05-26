#pragma once
#include <Utility.h>
#include <LMath.h>
class StaticMultiMeshRenderPass;
class SingleMeshRenderPass;

class SingleMeshRenderInstance {
public:
	SingleMeshRenderInstance();

	void loadInstance(const String& name, const VectorArray<String>& materialNames);
	
	//マテリアルをインデックスで取得
	RefPtr<SingleMeshRenderPass> getMaterial(uint32 index) const;

	//マテリアル配列を取得
	VectorArray<RefPtr<SingleMeshRenderPass>>& getMaterials();

	//このメッシュの描画用ワールド行列を更新
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<RefPtr<SingleMeshRenderPass>> _materials;
	Matrix4 _mtxWorld;
};

class StaticMultiMeshRender {
public:
	StaticMultiMeshRender(RefPtr<StaticMultiMeshRenderPass> rcg);
	RefPtr<StaticMultiMeshRenderPass> _rcg;
};