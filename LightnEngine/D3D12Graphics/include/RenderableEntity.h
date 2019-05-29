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
	
	//�}�e���A�����C���f�b�N�X�Ŏ擾
	RefPtr<SingleMeshRenderMaterial> getMaterial(uint32 index) const;

	//�}�e���A���z����擾
	VectorArray<RefPtr<SingleMeshRenderMaterial>>& getMaterials();

	//���̃��b�V���̕`��p���[���h�s����X�V
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