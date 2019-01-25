#pragma once

#include "stdafx.h"

constexpr UINT MAX_HEAP_NUM = 1024;

class BufferView {
public:
	virtual ~BufferView() {}

	D3D12_CPU_DESCRIPTOR_HANDLE _cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE _gpuHandle;
};

class RenderTargetView :public BufferView{

};

class ConstantBufferView :public BufferView{

};

class DescriptorHeap {
public:
	DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
	~DescriptorHeap();

	void create(ID3D12Device* device);

private:
	const D3D12_DESCRIPTOR_HEAP_TYPE _descriptorHeapType;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _descriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE _cpuHandleStart;
	D3D12_GPU_DESCRIPTOR_HANDLE _gpuHandleStart;
	UINT _incrimentSize;
};

class DescriptorHeapManager {
public:

	RenderTargetView * createRenderTargetView(ID3D12Resource* textureBuffer) {}
	ConstantBufferView* createConstantBufferView(ID3D12Resource* constantBuffer) {}

private:

	DescriptorHeap _rtvHeap;
	DescriptorHeap _dsvHeap;
	DescriptorHeap _srvCbvHeap;
};