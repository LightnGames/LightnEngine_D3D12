#pragma once
#include <Utility.h>
#include <LMath.h>
class StaticSingleMeshRCG;
class SharedMaterial;

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