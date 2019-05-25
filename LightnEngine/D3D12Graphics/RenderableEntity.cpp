#include "RenderableEntity.h"
#include "MeshRender.h"
#include "SharedMaterial.h"
#include "GpuResource.h"
#include "GpuResourceManager.h"

SingleMeshRenderInstance::SingleMeshRenderInstance() :_materials{}, _mtxWorld(Matrix4::identity) {
}

void SingleMeshRenderInstance::loadInstance(const String& name, const VectorArray<String>& materialNames){
	GpuResourceManager& manager = GpuResourceManager::instance();
	
	//メッシュインスタンスロード
	RefPtr<VertexAndIndexBuffer> vertexAndIndex;
	manager.loadVertexAndIndexBuffer(name, &vertexAndIndex);

	//マテリアルスロットにマテリアルをセット
	const size_t materialCount = vertexAndIndex->materialDrawRanges.size();
	_materials.resize(materialCount);

	for (size_t i = 0; i < materialNames.size(); ++i) {
		manager.loadSharedMaterial(materialNames[i], &_materials[i]);

		InstanceInfoPerMaterial drawInfo(&_mtxWorld, vertexAndIndex->getRefVertexAndIndexBuffer(i));
		_materials[i]->addMeshInstance(drawInfo);
	}
}

RefPtr<SharedMaterial> SingleMeshRenderInstance::getMaterial(uint32 index) const{
	return _materials[index];
}

VectorArray<RefPtr<SharedMaterial>>& SingleMeshRenderInstance::getMaterials(){
	return _materials;
}

void SingleMeshRenderInstance::updateWorldMatrix(const Matrix4& worldMatrix){
	_mtxWorld = worldMatrix;
}

StaticMultiMeshRender::StaticMultiMeshRender(RefPtr<StaticMultiMeshRCG> rcg):_rcg(rcg){
}
