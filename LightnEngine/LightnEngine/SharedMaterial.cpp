#include "SharedMaterial.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"

void ConstantBufferPerFrame::shutdown() {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	for (auto&& cbv : constantBufferViews) {
		if (cbv != nullptr) {
			descriptorHeapManager.discardConstantBufferView(cbv);
		}
	}

	for (uint32 i = 0; i < FrameCount; ++i) {
		for (auto&& cb : constantBuffers[i]) {
			cb.reset();
		}

		constantBuffers[i].clear();
	}
}

void ConstantBufferPerFrame::create(ID3D12Device * device, const VectorArray<uint32>& bufferSizes) {
	const uint32 constantBufferCount = static_cast<uint32>(bufferSizes.size());
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

	//フレームバッファリング枚数ｘ定数バッファの数だけ生成が必要
	for (int i = 0; i < FrameCount; ++i) {
		constantBuffers[i].resize(constantBufferCount);

		UniquePtr<ID3D12Resource*[]> constantBufferPtrs = makeUnique<ID3D12Resource*[]>(constantBufferCount);
		for (uint32 j = 0; j < constantBufferCount; ++j) {
			constantBuffers[i][j] = makeUnique<ConstantBuffer>();
			constantBuffers[i][j]->create(device, bufferSizes[j]);
			constantBufferPtrs[j] = constantBuffers[i][j]->get();
		}

		descriptorHeapManager.createConstantBufferView(constantBufferPtrs.get(), &constantBufferViews[i], constantBufferCount);
	}

	//定数バッファに書き込むための共有メモリ領域を確保
	dataPtrs.resize(constantBufferCount);
	for (uint32 i = 0; i < constantBufferCount; ++i) {
		dataPtrs[i] = UniquePtr<byte[]>(new byte[bufferSizes[i]]);
	}
}

void ConstantBufferPerFrame::flashBufferData(uint32 frameIndex) {
	for (size_t i = 0; i < constantBuffers[frameIndex].size(); ++i) {
		constantBuffers[frameIndex][i]->writeData(dataPtrs[i].get(), constantBuffers[frameIndex][i]->size);
	}
}

SharedMaterial::~SharedMaterial() {
	DescriptorHeapManager& manager = DescriptorHeapManager::instance();
	if (srvPixel != nullptr) {
		manager.discardShaderResourceView(srvPixel);
	}
}

void SharedMaterial::create(ID3D12Device * device, RefPtr<VertexShader> vertexShader, RefPtr<PixelShader> pixelShader) {
	rootSignature = makeUnique<RootSignature>();
	pipelineState = makeUnique<PipelineState>();
	rootSignature->create(device, *vertexShader, *pixelShader);
	pipelineState->create(device, rootSignature.get(), *vertexShader, *pixelShader);

	this->vertexShader = vertexShader;
	this->pixelShader = pixelShader;
}

void SharedMaterial::setupRenderCommand(RenderSettings & settings) {
	ID3D12GraphicsCommandList* commandList = settings.commandList;
	uint32 frameIndex = settings.frameIndex;

	commandList->SetPipelineState(pipelineState->_pipelineState.Get());
	commandList->SetGraphicsRootSignature(rootSignature->_rootSignature.Get());

	//定数バッファデータを現在のフレームで使用する定数バッファにコピー
	vertexConstantBuffer.flashBufferData(frameIndex);
	pixelConstantBuffer.flashBufferData(frameIndex);

	//このマテリアルで有効なリソースビューをセットする
	UINT descriptorTableIndex = 0;
	if (srvPixel != nullptr) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, srvPixel->gpuHandle);
		descriptorTableIndex++;
	}

	if (pixelConstantBuffer.isEnableBuffer()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, pixelConstantBuffer.constantBufferViews[frameIndex]->gpuHandle);
		descriptorTableIndex++;
	}

	if (srvVertex != nullptr) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, srvVertex->gpuHandle);
		descriptorTableIndex++;
	}

	if (vertexConstantBuffer.isEnableBuffer()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, vertexConstantBuffer.constantBufferViews[frameIndex]->gpuHandle);
		descriptorTableIndex++;
	}

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
