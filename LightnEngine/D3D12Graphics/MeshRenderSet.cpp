#include "MeshRenderSet.h"
#include "SharedMaterial.h"
#include "GpuResource.h"

void MeshRenderSet::setupRenderCommand(RenderSettings & settings) const{
	ID3D12GraphicsCommandList* commandList = settings.commandList;
	for (auto&& material : _materialSlots) {
		material.material->setupRenderCommand(settings);
		commandList->IASetVertexBuffers(0, 1, &_vertexBuffer->_vertexBufferView);
		commandList->IASetIndexBuffer(&_indexBuffer->_indexBufferView);
		commandList->DrawIndexedInstanced(material.indexCount, 1, material.indexOffset, 0, 0);
	}
}

RefPtr<SharedMaterial> MeshRenderSet::getMaterial(uint32 index) {
	return _materialSlots[index].material;
}
