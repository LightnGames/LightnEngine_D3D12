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

	//このインスタンスの末尾にマテリアルのデータをコピー
	//インスタンス生成時に適切なメモリを確保しておかないと即死亡！！！
	RefPtr<MaterialSlot> endPtr = getFirstMatrialPtr();
	memcpy(endPtr, materialSlots.data(), _materialSlotSize * sizeof(MaterialSlot));
}

void StaticSingleMeshRCG::setupRenderCommand(RenderSettings& settings) const{
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;

	commandList->IASetVertexBuffers(0, 1, &_vertexBufferView.view);
	commandList->IASetIndexBuffer(&_indexBufferView.view);

	//このインスタンスのすぐ後ろにマテリアルのコマンドが詰まっているのでまずは最初を取り出す
	RefPtr<MaterialSlot> material = getFirstMatrialPtr();

	for (size_t i = 0; i < _materialSlotSize; ++i) {
		settings.vertexRoot32bitConstants.emplace_back(static_cast<const void*>(&_worldMatrix), static_cast<uint32>(sizeof(_worldMatrix)));
		material->material.setupRenderCommand(settings);
		commandList->DrawIndexedInstanced(material->range.indexCount, 1, material->range.indexOffset, 0, 0);

		//ポインタをインクリメントして次のマテリアルを参照
		++material;
	}
}

void StaticSingleMeshRCG::updateWorldMatrix(const Matrix4& worldMatrix) {
	_worldMatrix = worldMatrix;
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
