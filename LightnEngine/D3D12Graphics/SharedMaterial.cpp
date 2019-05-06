#include "SharedMaterial.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"

ConstantBufferMaterial::ConstantBufferMaterial() :constantBufferViews{} {
}

ConstantBufferMaterial::~ConstantBufferMaterial() {
	//shutdown();
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

void ConstantBufferMaterial::writeBufferData(const void* dataPtr, uint32 length, uint32 bufferIndex) {
	memcpy(dataPtrs[bufferIndex], dataPtr, length);
}

Root32bitConstantMaterial::~Root32bitConstantMaterial(){
	for (auto&& data : dataPtrs) {
		delete data;
	}
}

void Root32bitConstantMaterial::create(const VectorArray<uint32>& bufferSizes) {
	dataPtrs.resize(bufferSizes.size());

	for (size_t i = 0; i < bufferSizes.size(); ++i) {
		dataPtrs[i] = new byte[bufferSizes[i]];
	}
}

bool Root32bitConstantMaterial::isEnableConstant() const {
	return !dataPtrs.empty();
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

	auto& pixelRoot32bitConstants = settings.pixelRoot32bitConstants;
	auto& vertexRoot32bitConstants = settings.vertexRoot32bitConstants;

	commandList->SetPipelineState(_pipelineState.pipelineState);
	commandList->SetGraphicsRootSignature(_rootSignature.rootSignature);

	//このマテリアルで有効なリソースビューをセットする
	UINT descriptorTableIndex = 0;
	if (_srvPixel.isEnable()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, _srvPixel.gpuHandle);
		descriptorTableIndex++;
	}

	if (_pixelConstantBuffer.isEnableBuffer()) {
		commandList->SetGraphicsRootDescriptorTable(descriptorTableIndex, _pixelConstantBuffer.constantBufferViews[frameIndex].gpuHandle);
		descriptorTableIndex++;
	}

	bool isPixelRoot32bitOverride = !pixelRoot32bitConstants.empty();
	for (size_t i = 0; i < _psReflection.root32bitConstants.size(); ++i) {
		const void* ptr = nullptr;
		if (isPixelRoot32bitOverride) {
			ptr = pixelRoot32bitConstants[i].first;
		}
		else {
			ptr = _pixelRoot32bitConstant.dataPtrs[i];
		}

		commandList->SetGraphicsRoot32BitConstants(descriptorTableIndex, _psReflection.getRoot32bitConstantNum(static_cast<uint32>(i)), ptr, 0);
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

	bool isVertexRoot32bitOverride = !vertexRoot32bitConstants.empty();
	for (size_t i = 0; i < _vsReflection.root32bitConstants.size(); ++i) {
		const void* ptr = nullptr;
		if (isVertexRoot32bitOverride) {
			ptr = vertexRoot32bitConstants[i].first;
		}
		else {
			ptr = _vertexRoot32bitConstant.dataPtrs[i];
		}

		commandList->SetGraphicsRoot32BitConstants(descriptorTableIndex, _vsReflection.getRoot32bitConstantNum(static_cast<uint32>(i)), ptr, 0);
		descriptorTableIndex++;
	}

	commandList->IASetPrimitiveTopology(_topology);

	pixelRoot32bitConstants.clear();
	vertexRoot32bitConstants.clear();
}