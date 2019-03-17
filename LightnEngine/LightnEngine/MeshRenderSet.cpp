#include "MeshRenderSet.h"
#include "SharedMaterial.h"
#include "GpuResource.h"

void MeshRenderSet::setupRenderCommand(RenderSettings & settings) {
	ID3D12GraphicsCommandList* commandList = settings.commandList;
	for (auto&& material : materialSlots) {
		material.material->setupRenderCommand(settings);
		commandList->IASetVertexBuffers(0, 1, &vertexBuffer->_vertexBufferView);
		commandList->IASetIndexBuffer(&indexBuffer->_indexBufferView);
		commandList->DrawIndexedInstanced(material.indexCount, 1, material.indexOffset, 0, 0);
	}
}

RefPtr<SharedMaterial> MeshRenderSet::getMaterial(uint32 index) {
	return materialSlots[index].material;
}
