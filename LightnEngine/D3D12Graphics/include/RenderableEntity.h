#pragma once
#include <Utility.h>
#include <LMath.h>
class StaticMultiMeshRCG;
class SharedMaterial;

class SingleMeshRenderInstance {
public:
	SingleMeshRenderInstance();

	void loadInstance(const String& name, const VectorArray<String>& materialNames);
	
	//�}�e���A�����C���f�b�N�X�Ŏ擾
	RefPtr<SharedMaterial> getMaterial(uint32 index) const;

	//�}�e���A���z����擾
	VectorArray<RefPtr<SharedMaterial>>& getMaterials();

	//���̃��b�V���̕`��p���[���h�s����X�V
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<RefPtr<SharedMaterial>> _materials;
	Matrix4 _mtxWorld;
};

class StaticMultiMeshRender {
public:
	StaticMultiMeshRender(RefPtr<StaticMultiMeshRCG> rcg);
	RefPtr<StaticMultiMeshRCG> _rcg;
};