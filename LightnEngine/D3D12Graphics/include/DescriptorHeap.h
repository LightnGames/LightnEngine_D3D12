#pragma once

#include "stdafx.h"
#include "Utility.h"

constexpr UINT MAX_HEAP_NUM = 1024;
class GenericAllocator;

struct BufferView {
	virtual ~BufferView() {}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	ulong2 location;
	uint32 size;
};

class DescriptorHeap {
public:
	DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
	~DescriptorHeap();

	void create(RefPtr<ID3D12Device> device, uint32 maxDescriptorCount = MAX_HEAP_NUM);
	void shutdown();

	RefPtr<BufferView> allocateBufferView(uint32 descriptorCount);
	void discardBufferView(RefPtr<BufferView> bufferView);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(ulong2 bufferViewLocation) const;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(ulong2 bufferViewLocation) const;

	RefPtr<ID3D12DescriptorHeap> descriptorHeap() { return _descriptorHeap.Get(); }
	UINT incrimentSize() const { return _incrimentSize; }

private:
	const D3D12_DESCRIPTOR_HEAP_TYPE _descriptorHeapType;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _descriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE _cpuHandleStart;
	D3D12_GPU_DESCRIPTOR_HANDLE _gpuHandleStart;
	UINT _incrimentSize;


	GenericAllocator* _allocator;
};

class DescriptorHeapManager : public Singleton<DescriptorHeapManager> {
public:
	DescriptorHeapManager();
	~DescriptorHeapManager();

	void create(RefPtr<ID3D12Device> device);
	void shutdown();

	void createRenderTargetView(ID3D12Resource** textureBuffers, BufferView** dstView, uint32 viewCount);
	void createConstantBufferView(ID3D12Resource** constantBuffers, BufferView** dstView, uint32 viewCount);
	void createShaderResourceView(ID3D12Resource** shaderResources, BufferView** dstView, uint32 viewCount);
	void createDepthStencilView(ID3D12Resource** depthStencils, BufferView** dstView, uint32 viewCount);

	void discardRenderTargetView(BufferView* bufferView);
	void discardConstantBufferView(BufferView* bufferView);
	void discardShaderResourceView(BufferView* bufferView);
	void discardDepthStencilView(BufferView* bufferView);

	//Direct3Dのデスクリプタヒープをタイプから直接取得
	RefPtr<ID3D12DescriptorHeap> getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
	RefPtr<DescriptorHeap> getDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);

private:

	DescriptorHeap _rtvHeap;
	DescriptorHeap _dsvHeap;
	DescriptorHeap _cbvSrvHeap;
};