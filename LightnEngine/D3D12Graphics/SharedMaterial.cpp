#include "SharedMaterial.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"
#include "MeshRender.h"

ConstantBufferFrame::ConstantBufferFrame() :constantBufferViews{} {
}

ConstantBufferFrame::~ConstantBufferFrame() {
}

void ConstantBufferFrame::shutdown() {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	for (const auto& cbv : constantBufferViews) {
		if (cbv.isEnable()) {
			descriptorHeapManager.discardConstantBufferView(cbv);
		}
	}

	for (uint32 i = 0; i < FrameCount; ++i) {
		for (auto&& cb : constantBuffers[i]) {
			delete cb;
		}

		constantBuffers[i].clear();
	}

	for (auto&& data : dataPtrs) {
		delete data;
	}
}

void ConstantBufferFrame::create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes) {
	const uint32 constantBufferCount = static_cast<uint32>(bufferSizes.size());
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

	//�t���[���o�b�t�@�����O�������萔�o�b�t�@�̐������������K�v
	for (int i = 0; i < FrameCount; ++i) {
		constantBuffers[i].resize(constantBufferCount);

		UniquePtr<ID3D12Resource* []> constantBufferPtrs = makeUnique<ID3D12Resource * []>(constantBufferCount);
		for (uint32 j = 0; j < constantBufferCount; ++j) {
			constantBuffers[i][j] = new ConstantBuffer();
			constantBuffers[i][j]->create(device, bufferSizes[j]);
			constantBufferPtrs[j] = constantBuffers[i][j]->get();
		}

		descriptorHeapManager.createConstantBufferView(constantBufferPtrs.get(), &constantBufferViews[i], constantBufferCount);
	}

	//�萔�o�b�t�@�ɏ������ނ��߂̋��L�������̈���m��
	dataPtrs.resize(constantBufferCount);
	for (uint32 i = 0; i < constantBufferCount; ++i) {
		dataPtrs[i] = new byte[bufferSizes[i]];
	}
}

bool ConstantBufferFrame::isEnableBuffer() const {
	return !constantBuffers[0].empty();
}

void ConstantBufferFrame::flashBufferData(uint32 frameIndex) {
	for (size_t i = 0; i < constantBuffers[frameIndex].size(); ++i) {
		constantBuffers[frameIndex][i]->writeData(dataPtrs[i], constantBuffers[frameIndex][i]->size);
	}
}

void ConstantBufferFrame::writeBufferData(const void* dataPtr, uint32 length, uint32 bufferIndex) {
	memcpy(dataPtrs[bufferIndex], dataPtr, length);
}

SharedMaterial::SharedMaterial(
	const ShaderReflectionResult& vsReflection,
	const ShaderReflectionResult& psReflection,
	const RefPipelineState& pipelineState,
	const RefRootsignature& rootSignature,
	D3D_PRIMITIVE_TOPOLOGY topology)
	:_vsReflection(vsReflection),
	_psReflection(psReflection),
	_pipelineState(pipelineState),
	_rootSignature(rootSignature),
	_srvVertex(),
	_srvPixel(),
	_topology(topology) {
}

SharedMaterial::~SharedMaterial() {
	DescriptorHeapManager& manager = DescriptorHeapManager::instance();
	if (_srvPixel.isEnable()) {
		manager.discardShaderResourceView(_srvPixel);
	}

	if (_srvVertex.isEnable()) {
		manager.discardShaderResourceView(_srvVertex);
	}

	_vertexConstantBuffer.shutdown();
	_pixelConstantBuffer.shutdown();
}

void SharedMaterial::setupRenderCommand(RenderSettings& settings) const{
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;

	commandList->SetPipelineState(_pipelineState.pipelineState);
	commandList->SetGraphicsRootSignature(_rootSignature.rootSignature);

	//���̃}�e���A���ŗL���ȃ��\�[�X�r���[���Z�b�g����
	UINT descriptorTableIndex = 0;
	if (_srvPixel.isEnable()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, _srvPixel.gpuHandle);
		descriptorTableIndex++;
	}

	if (_pixelConstantBuffer.isEnableBuffer()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, _pixelConstantBuffer.constantBufferViews[frameIndex].gpuHandle);
		descriptorTableIndex++;
	}

	if (_srvVertex.isEnable()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, _srvVertex.gpuHandle);
		descriptorTableIndex++;
	}

	if (_vertexConstantBuffer.isEnableBuffer()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, _vertexConstantBuffer.constantBufferViews[frameIndex].gpuHandle);
		descriptorTableIndex++;
	}

	commandList->IASetPrimitiveTopology(_topology);

	uint32 cnt = 0;
	for (auto&& mesh : _meshes) {
		D3D12_VERTEX_BUFFER_VIEW views[2] = { mesh->vertexView.view, _instanceVertexView };
		commandList->IASetVertexBuffers(0, 2, views);
		commandList->IASetIndexBuffer(&mesh->indexView.view);
		commandList->DrawIndexedInstanced(mesh->drawRange.indexCount, 1, mesh->drawRange.indexOffset, 0, cnt);
		
		cnt++;
	}
}

void SharedMaterial::addMesh(RefPtr<ID3D12Device> device, VectorArray<RefPtr<Matrix4>> matrices, VectorArray<RefPtr<RefVertexAndIndexBuffer>>& meshes) {
	_matrices = matrices;
	_meshes = meshes;

	VectorArray<Matrix4> m;
	m.reserve(_matrices.size());
	for (const auto& matrix : _matrices) {
		m.emplace_back(*matrix);
	}

	_instanceVertexBuffer.createDirectCpuReadWrite<Matrix4>(device, m);
	_instanceVertexView.BufferLocation = _instanceVertexBuffer.getGpuVirtualAddress();
	_instanceVertexView.SizeInBytes = sizeof(Matrix4) * _matrices.size();
	_instanceVertexView.StrideInBytes = sizeof(Matrix4);
}
