#pragma once
#include <Utility.h>
#include <LMath.h>
class StaticMultiMeshRenderPass;
class SingleMeshRenderPass;

class SingleMeshRenderInstance {
public:
	SingleMeshRenderInstance();

	void loadInstance(const String& name, const VectorArray<String>& materialNames);
	
	//�}�e���A�����C���f�b�N�X�Ŏ擾
	RefPtr<SingleMeshRenderPass> getMaterial(uint32 index) const;

	//�}�e���A���z����擾
	VectorArray<RefPtr<SingleMeshRenderPass>>& getMaterials();

	//���̃��b�V���̕`��p���[���h�s����X�V
	void updateWorldMatrix(const Matrix4& worldMatrix);

	VectorArray<RefPtr<SingleMeshRenderPass>> _materials;
	Matrix4 _mtxWorld;
};

class StaticMultiMeshRender {
public:
	StaticMultiMeshRender(RefPtr<StaticMultiMeshRenderPass> rcg);
	RefPtr<StaticMultiMeshRenderPass> _rcg;
};