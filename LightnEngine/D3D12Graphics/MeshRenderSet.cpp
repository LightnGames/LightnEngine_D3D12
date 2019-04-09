#include "MeshRenderSet.h"
#include "SharedMaterial.h"
#include "GpuResource.h"

MeshRenderSet::MeshRenderSet(UniquePtr<VertexBuffer> vertexBuffer, UniquePtr<IndexBuffer> indexBuffer, const VectorArray<MaterialSlot>& materialSlots) :
	_vertexBuffer(std::move(vertexBuffer)), _indexBuffer(std::move(indexBuffer)), _materialSlots(materialSlots) {
}

void MeshRenderSet::setupRenderCommand(RenderSettings & settings) const{
	ID3D12GraphicsCommandList* commandList = settings.commandList;
	for (auto&& material : _materialSlots) {
		material.material->setupRenderCommand(settings);
		commandList->IASetVertexBuffers(0, 1, &_vertexBuffer->_vertexBufferView);
		commandList->IASetIndexBuffer(&_indexBuffer->_indexBufferView);
		commandList->DrawIndexedInstanced(material.indexCount, 1, material.indexOffset, 0, 0);
	}
}

void MeshRenderSet::setMaterial(uint32 index, RefPtr<SharedMaterial> material){
	_materialSlots[index].material = material;
}

RefPtr<SharedMaterial> MeshRenderSet::getMaterial(uint32 index) {
	return _materialSlots[index].material;
}

RefPtr<MaterialSlot> MeshRenderSet::getMaterialSlot(uint32 index){
	return &_materialSlots[index];
}
