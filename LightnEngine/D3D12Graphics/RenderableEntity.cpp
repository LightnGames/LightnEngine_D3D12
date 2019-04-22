#include "RenderableEntity.h"
#include "SharedMaterial.h"
#include "GpuResource.h"

StaticSingleMeshRCG::StaticSingleMeshRCG(
	const RefVertexBufferView& vertexBufferView,
	const RefIndexBufferView& indexBufferView,
	const VectorArray<MaterialSlot>& materialSlots) :
	_vertexBufferView(vertexBufferView),
	_indexBufferView(indexBufferView),
	_materialSlotSize(materialSlots.size()),
	_worldMatrix(Matrix4::identity) {

	//���̃C���X�^���X�̖����Ƀ}�e���A���̃f�[�^���R�s�[
	//�C���X�^���X�������ɓK�؂ȃ��������m�ۂ��Ă����Ȃ��Ƒ����S�I�I�I
	RefPtr<MaterialSlot> endPtr = getFirstMatrialPtr();
	memcpy(endPtr, materialSlots.data(), _materialSlotSize * sizeof(MaterialSlot));
}

void StaticSingleMeshRCG::setupRenderCommand(RenderSettings& settings) const{
	settings.vertexRoot32bitConstants.emplace_back(static_cast<const void*>(&_worldMatrix), static_cast<uint32>(sizeof(_worldMatrix)));
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;

	commandList->IASetVertexBuffers(0, 1, &_vertexBufferView.view);
	commandList->IASetIndexBuffer(&_indexBufferView.view);

	RefPtr<MaterialSlot> material = getFirstMatrialPtr();

	for (size_t i = 0; i < _materialSlotSize; ++i) {
		material->material.setupRenderCommand(settings);
		commandList->DrawIndexedInstanced(material->range.indexCount, 1, material->range.indexOffset, 0, 0);

		++material;
	}

	//for (const auto& material : _materialSlots) {
	//	material.material.setupRenderCommand(settings);
	//	commandList->DrawIndexedInstanced(material.range.indexCount, 1, material.range.indexOffset, 0, 0);
	//}
}

void StaticSingleMeshRCG::updateWorldMatrix(const Matrix4& worldMatrix) {
	_worldMatrix = worldMatrix;
}

RefPtr<SharedMaterial> StaticSingleMeshRender::getMaterial(uint32 index) const{
	return _materials[index];
}

void StaticSingleMeshRender::updateWorldMatrix(const Matrix4& worldMatrix){
	_rcg->updateWorldMatrix(worldMatrix);
}
