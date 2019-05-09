#include "RenderableEntity.h"
#include "MeshRender.h"
#include "SharedMaterial.h"
#include "GpuResource.h"

StaticSingleMeshRender::StaticSingleMeshRender() :_materials{}, _rcg(nullptr) {
}

StaticSingleMeshRender::StaticSingleMeshRender(const VectorArray<RefPtr<SharedMaterial>>& materials, RefPtr<StaticSingleMeshRCG> rcg) :
	_materials(materials), _rcg(rcg) {
}

RefPtr<SharedMaterial> StaticSingleMeshRender::getMaterial(uint32 index) const{
	return _materials[index];
}

VectorArray<RefPtr<SharedMaterial>>& StaticSingleMeshRender::getMaterials(){
	return _materials;
}

void StaticSingleMeshRender::updateWorldMatrix(const Matrix4& worldMatrix){
	_rcg->updateWorldMatrix(worldMatrix);
}
