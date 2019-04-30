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
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;

	commandList->IASetVertexBuffers(0, 1, &_vertexBufferView.view);
	commandList->IASetIndexBuffer(&_indexBufferView.view);

	//���̃C���X�^���X�̂������Ƀ}�e���A���̃R�}���h���l�܂��Ă���̂ł܂��͍ŏ������o��
	RefPtr<MaterialSlot> material = getFirstMatrialPtr();

	for (size_t i = 0; i < _materialSlotSize; ++i) {
		const RefSharedMaterial& mat = material->material;

		commandList->SetPipelineState(mat.pipelineState.pipelineState);
		commandList->SetGraphicsRootSignature(mat.rootSignature.rootSignature);

		//�s�N�Z���V�F�[�_�[���\�[�X�r���[���}�b�v
		const RefBufferView& srvPixel = mat.srvPixel;
		if (srvPixel.isEnable()) {
			commandList->SetGraphicsRootDescriptorTable(srvPixel.descriptorIndex, srvPixel.gpuHandle);
		}

		//�s�N�Z���萔�o�b�t�@���}�b�v
		if (mat.pixelConstantViews.isEnableBuffers()) {
			const RefBufferView& view = mat.pixelConstantViews.views[frameIndex];
			commandList->SetGraphicsRootDescriptorTable(view.descriptorIndex, view.gpuHandle);
		}

		//for (uint32 i = 0; i < mat.pixelRoot32bitConstantCount; ++i) {
		//	commandList->SetGraphicsRoot32BitConstants(mat.pixelRoot32bitConstantIndex + i, settings.pixelRoot32bitConstants[i].second / 4, settings.pixelRoot32bitConstants[i].first, 0);
		//}

		//���_�V�F�[�_�[���\�[�X���}�b�v
		const RefBufferView& srvVertex = mat.srvVertex;
		if (srvVertex.isEnable()) {
			commandList->SetGraphicsRootDescriptorTable(srvVertex.descriptorIndex, srvVertex.gpuHandle);
		}

		//���_�萔�o�b�t�@���}�b�v
		if (mat.vertexConstantViews.isEnableBuffers()) {
			const RefBufferView& view = mat.vertexConstantViews.views[frameIndex];
			commandList->SetGraphicsRootDescriptorTable(view.descriptorIndex, view.gpuHandle);
		}

		//�V���O���X�^�e�B�b�N���b�V���p�̃��[���h�s����}�b�v
		commandList->SetGraphicsRoot32BitConstants(mat.vertexRoot32bitConstantIndex, static_cast<uint32>(sizeof(_worldMatrix) / 4), &_worldMatrix, 0);

		//�h���[�R�[��
		commandList->IASetPrimitiveTopology(mat.topology);
		commandList->DrawIndexedInstanced(material->range.indexCount, 1, material->range.indexOffset, 0, 0);

		//�|�C���^���C���N�������g���Ď��̃}�e���A�����Q��
		++material;
	}
}

void StaticSingleMeshRCG::updateWorldMatrix(const Matrix4& worldMatrix) {
	_worldMatrix = worldMatrix;
}

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
