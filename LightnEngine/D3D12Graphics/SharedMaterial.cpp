#include "SharedMaterial.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"

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
		constantBuffers[i].destroy();
	}

	dataPtrs.clear();
}

void ConstantBufferFrame::create(RefPtr<ID3D12Device> device, uint32 bufferSize) {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

	//フレームバッファリング枚数ｘ定数バッファの数だけ生成が必要
	for (int i = 0; i < FrameCount; ++i) {
		constantBuffers[i].create(device, bufferSize);
		descriptorHeapManager.createConstantBufferView(constantBuffers[i].getAdressOf(), &constantBufferViews[i], 1);
	}

	//定数バッファに書き込むための共有メモリ領域を確保
	dataPtrs.resize(bufferSize);
}

bool ConstantBufferFrame::isEnableBuffer() const {
	return !constantBuffers[0].size != 0;
}

void ConstantBufferFrame::flashBufferData(uint32 frameIndex) {
	constantBuffers[frameIndex].writeData(dataPtrs.data(), constantBuffers[frameIndex].size);
}

void ConstantBufferFrame::writeBufferData(const void* dataPtr, uint32 length) {
	memcpy(dataPtrs.data(), dataPtr, length);
}

void ConstantBufferFrame::getVirtualAdresses(RefPtr<D3D12_GPU_VIRTUAL_ADDRESS> dstArray) const{
	dstArray[0] = constantBuffers[0].getGpuVirtualAddress();
	dstArray[1] = constantBuffers[1].getGpuVirtualAddress();
	dstArray[2] = constantBuffers[2].getGpuVirtualAddress();
}
