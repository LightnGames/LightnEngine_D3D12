#pragma once
#include <Utility.h>
#include <LMath.h>
class StaticMultiMeshRCG;
class SharedMaterial;

class SingleMeshRenderInstance {
public:
	SingleMeshRenderInstance();

	void loadInstance(const String& name, const VectorArray<String>& materialNames);
	
	//マテリアルをインデックスで取得
	RefPtr<SharedMaterial> getMaterial(uint32 index) const;

	//マテリアル配列を取得
	VectorArray<RefPtr<SharedMaterial>>& getMaterials();

	//このメッシュの描画用ワールド行列を更新
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<RefPtr<SharedMaterial>> _materials;
	Matrix4 _mtxWorld;
};

class StaticMultiMeshRender {
public:
	StaticMultiMeshRender(RefPtr<StaticMultiMeshRCG> rcg);
	RefPtr<StaticMultiMeshRCG> _rcg;
};