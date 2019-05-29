#include "RenderableEntity.h"
#include "SharedMaterial.h"
#include "GpuResource.h"
#include "GpuResourceManager.h"
#include "GraphicsCore.h"

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
}

RefPtr<SingleMeshRenderMaterial> SingleMeshRenderInstance::getMaterial(uint32 index) const{
	return _materials[index];
}

VectorArray<RefPtr<SingleMeshRenderMaterial>>& SingleMeshRenderInstance::getMaterials(){
	return _materials;
}

void SingleMeshRenderInstance::updateWorldMatrix(const Matrix4& worldMatrix){
	_mtxWorld = worldMatrix.transpose();
	_mesh->_mtxWorld = _mtxWorld.transpose();
}

StaticMultiMeshRenderInstance::StaticMultiMeshRenderInstance(RefPtr<StaticMultiMeshMaterial> rcg):_rcg(rcg){
}
