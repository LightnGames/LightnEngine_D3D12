#include "DescriptorHeap.h"
#include "D3D12Helper.h"
#include "D3D12Util.h"

DescriptorHeap::DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type):_descriptorHeapType(type) {
}


DescriptorHeap::~DescriptorHeap() {
}

void DescriptorHeap::create(ID3D12Device * device) {
	D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (_descriptorHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
		flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = MAX_HEAP_NUM;
	heapDesc.Type = _descriptorHeapType;
	heapDesc.Flags = flags;
	throwIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_descriptorHeap)));
	NAME_D3D12_OBJECT(_descriptorHeap);

	_incrimentSize = device->GetDescriptorHandleIncrementSize(_descriptorHeapType);
	_cpuHandleStart = _descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	_gpuHandleStart = _descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}
