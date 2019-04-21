#include "RenderableEntity.h"
#include "SharedMaterial.h"
#include "GpuResource.h"

StaticSingleMeshRender::StaticSingleMeshRender(
	const RefVertexBufferView& vertexBufferView,
	const RefIndexBufferView& indexBufferView,
	const VectorArray<MaterialSlot>& materialSlots) :
	_vertexBufferView(vertexBufferView),
	_indexBufferView(indexBufferView),
	_materialSlots(materialSlots),
	_worldMatrix(Matrix4::identity) {
}

void StaticSingleMeshRender::setupRenderCommand(RenderSettings& settings) const{
	settings.vertexRoot32bitConstants.emplace_back((void*)&_worldMatrix);
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;

	commandList->IASetVertexBuffers(0, 1, &_vertexBufferView.view);
	commandList->IASetIndexBuffer(&_indexBufferView.view);

	for (auto&& material : _materialSlots) {
		material.material->setupRenderCommand(settings);
		commandList->DrawIndexedInstanced(material.indexCount, 1, material.indexOffset, 0, 0);
	}
}

void StaticSingleMeshRender::updateWorldMatrix(const Matrix4& worldMatrix) {
	_worldMatrix = worldMatrix;
}

void StaticSingleMeshRender::setMaterial(uint32 index, RefPtr<SharedMaterial> material) {
	_materialSlots[index].material = material;
}

RefPtr<SharedMaterial> StaticSingleMeshRender::getMaterial(uint32 index) const{
	return _materialSlots[index].material;
}
