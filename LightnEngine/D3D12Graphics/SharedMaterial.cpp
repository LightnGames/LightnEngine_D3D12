#include "SharedMaterial.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"

ConstantBufferMaterial::ConstantBufferMaterial() :constantBufferViews{} {
}

ConstantBufferMaterial::~ConstantBufferMaterial() {
	shutdown();
}

void ConstantBufferMaterial::shutdown() {
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

void ConstantBufferMaterial::create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes) {
	const uint32 constantBufferCount = static_cast<uint32>(bufferSizes.size());
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

	//フレームバッファリング枚数ｘ定数バッファの数だけ生成が必要
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

	//定数バッファに書き込むための共有メモリ領域を確保
	dataPtrs.resize(constantBufferCount);
	for (uint32 i = 0; i < constantBufferCount; ++i) {
		dataPtrs[i] = new byte[bufferSizes[i]];
	}
}

bool ConstantBufferMaterial::isEnableBuffer() const {
	return !constantBuffers[0].empty();
}

void ConstantBufferMaterial::flashBufferData(uint32 frameIndex) {
	for (size_t i = 0; i < constantBuffers[frameIndex].size(); ++i) {
		constantBuffers[frameIndex][i]->writeData(dataPtrs[i], constantBuffers[frameIndex][i]->size);
	}
}

Root32bitConstantMaterial::~Root32bitConstantMaterial(){
	for (auto&& data : dataPtrs) {
		delete data;
	}
}

void Root32bitConstantMaterial::create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes) {
	dataPtrs.resize(bufferSizes.size());

	for (size_t i = 0; i < bufferSizes.size(); ++i) {
		dataPtrs[i] = new byte[bufferSizes[i]];
	}
}

bool Root32bitConstantMaterial::isEnableConstant() const {
	return !dataPtrs.empty();
}

SharedMaterial::SharedMaterial(RefPtr<VertexShader> vertexShader, RefPtr<PixelShader> pixelShader, RefPtr<PipelineState> pipelineState, RefPtr<RootSignature> rootSignature)
	:vertexShader(vertexShader), pixelShader(pixelShader), pipelineState(pipelineState), rootSignature(rootSignature), srvVertex(), srvPixel() {
}

SharedMaterial::~SharedMaterial() {
	DescriptorHeapManager& manager = DescriptorHeapManager::instance();
	if (srvPixel.isEnable()) {
		manager.discardShaderResourceView(srvPixel);
	}

	if (srvVertex.isEnable()) {
		manager.discardShaderResourceView(srvVertex);
	}
}

void SharedMaterial::setupRenderCommand(RenderSettings& settings) {
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;

	commandList->SetPipelineState(pipelineState->_pipelineState.Get());
	commandList->SetGraphicsRootSignature(rootSignature->_rootSignature.Get());

	//定数バッファデータを現在のフレームで使用する定数バッファにコピー
	vertexConstantBuffer.flashBufferData(frameIndex);
	pixelConstantBuffer.flashBufferData(frameIndex);

	const ShaderReflectionResult& pixelShaderResult = pixelShader->shaderReflectionResult;
	const ShaderReflectionResult& vertexShaderResult = vertexShader->shaderReflectionResult;

	//このマテリアルで有効なリソースビューをセットする
	UINT descriptorTableIndex = 0;
	if (srvPixel.isEnable()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, srvPixel.gpuHandle);
		descriptorTableIndex++;
	}

	if (pixelConstantBuffer.isEnableBuffer()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, pixelConstantBuffer.constantBufferViews[frameIndex].gpuHandle);
		descriptorTableIndex++;
	}

	bool isPixelRoot32bitOverride = !settings.pixelRoot32bitConstants.empty();
	for (size_t i = 0; i < pixelShaderResult.root32bitConstants.size(); ++i) {
		void* ptr = nullptr;
		if (isPixelRoot32bitOverride) {
			ptr = settings.pixelRoot32bitConstants[i];
		}
		else {
			ptr = pixelRoot32bitConstant.dataPtrs[i];
		}

		commandList->SetGraphicsRoot32BitConstants(descriptorTableIndex, pixelShaderResult.getRoot32bitConstantNum(static_cast<uint32>(i)), ptr, 0);
		descriptorTableIndex++;
	}

	if (srvVertex.isEnable()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, srvVertex.gpuHandle);
		descriptorTableIndex++;
	}

	if (vertexConstantBuffer.isEnableBuffer()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, vertexConstantBuffer.constantBufferViews[frameIndex].gpuHandle);
		descriptorTableIndex++;
	}

	bool isVertexRoot32bitOverride = !settings.vertexRoot32bitConstants.empty();
	for (size_t i = 0; i < vertexShaderResult.root32bitConstants.size(); ++i) {
		void* ptr = nullptr;
		if (isVertexRoot32bitOverride) {
			ptr = settings.vertexRoot32bitConstants[i];
		}
		else {
			ptr = vertexRoot32bitConstant.dataPtrs[i];
		}

		commandList->SetGraphicsRoot32BitConstants(descriptorTableIndex, vertexShaderResult.getRoot32bitConstantNum(static_cast<uint32>(i)), ptr, 0);
		descriptorTableIndex++;
	}

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	settings.pixelRoot32bitConstants.clear();
	settings.vertexRoot32bitConstants.clear();
}