#pragma once
#include <Utility.h>
#include <LMath.h>
class StaticMultiMeshMaterial;
class SingleMeshRenderMaterial;
class StaticSingleMesh;

class SingleMeshRenderInstance {
public:
	SingleMeshRenderInstance();

	void loadInstance(const String& name, const VectorArray<String>& materialNames);
	
	//マテリアルをインデックスで取得
	RefPtr<SingleMeshRenderMaterial> getMaterial(uint32 index) const;

	//マテリアル配列を取得
	VectorArray<RefPtr<SingleMeshRenderMaterial>>& getMaterials();

	//このメッシュの描画用ワールド行列を更新
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<RefPtr<SingleMeshRenderMaterial>> _materials;
	RefPtr<StaticSingleMesh> _mesh;
	Matrix4 _mtxWorld;
};

class StaticMultiMeshRenderInstance {
public:
	StaticMultiMeshRenderInstance(RefPtr<StaticMultiMeshMaterial> rcg);
	RefPtr<StaticMultiMeshMaterial> _rcg;
};