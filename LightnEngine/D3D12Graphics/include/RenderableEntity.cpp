#include "RenderableEntity.h"
#include <MeshRenderSet.h>
#include <SharedMaterial.h>
#include <GpuResource.h>

StaticSingleMeshRender::StaticSingleMeshRender(RefPtr<MeshRenderSet> renderSet) :
	_renderSet(renderSet), _worldMatrix(Matrix4::identity) {
}

void StaticSingleMeshRender::init(){
}

void StaticSingleMeshRender::setupRenderCommand(RenderSettings& settings) const {
	settings.vertexRoot32bitConstants.emplace_back((void*)&_worldMatrix);
	_renderSet->setupRenderCommand(settings);
}

void StaticSingleMeshRender::updateWorldMatrix(const Matrix4& worldMatrix) {
	_worldMatrix = worldMatrix;
}

void StaticSingleMeshRender::setMaterial(uint32 index, RefPtr<SharedMaterial> material) {
	_renderSet->setMaterial(index, material);
}

RefPtr<SharedMaterial> StaticSingleMeshRender::getMaterial(uint32 index) {
	return _renderSet->getMaterial(index);
}
