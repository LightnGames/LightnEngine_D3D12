#pragma once
#include <Utility.h>
#include <LMath.h>
class StaticSingleMeshRCG;
class SharedMaterial;

class StaticSingleMeshRender {
public:
	StaticSingleMeshRender();
	StaticSingleMeshRender(const VectorArray<RefPtr<SharedMaterial>>& materials, RefPtr<StaticSingleMeshRCG> rcg);
	
	//�}�e���A�����C���f�b�N�X�Ŏ擾
	RefPtr<SharedMaterial> getMaterial(uint32 index) const;

	//�}�e���A���z����擾
	VectorArray<RefPtr<SharedMaterial>>& getMaterials();

	//���̃��b�V���̕`��p���[���h�s����X�V
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<RefPtr<SharedMaterial>> _materials;
	RefPtr<StaticSingleMeshRCG> _rcg;
};