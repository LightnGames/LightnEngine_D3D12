#pragma once

#include "stdafx.h"
#include "Utility.h"
#include "BufferView.h"

constexpr UINT MAX_HEAP_NUM = 1024;
class GenericAllocator;

class DescriptorHeap {
public:
	explicit DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
	~DescriptorHeap();

	void create(RefPtr<ID3D12Device> device, uint32 maxDescriptorCount = MAX_HEAP_NUM);
	void shutdown();

	void allocateBufferView(RefPtr<BufferView> bufferView, uint32 descriptorCount);
	void discardBufferView(const BufferView& bufferView);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(ulong2 bufferViewLocation) const;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(ulong2 bufferViewLocation) const;

	RefPtr<ID3D12DescriptorHeap> descriptorHeap() const;
	UINT incrimentSize() const;

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

	void createRenderTargetView(RefAddressOf<ID3D12Resource> textureBuffers, RefPtr<BufferView> dstView, uint32 viewCount);
	void createConstantBufferView(RefAddressOf<ID3D12Resource> constantBuffers, RefPtr<BufferView> dstView, uint32 viewCount);
	void createShaderResourceView(RefAddressOf<ID3D12Resource> shaderResources, RefPtr<BufferView> dstView, uint32 viewCount, const D3D12_BUFFER_SRV& buffer);
	void createTextureShaderResourceView(RefAddressOf<ID3D12Resource> textureResources, RefPtr<BufferView> dstView, uint32 viewCount);
	void createDepthStencilView(RefAddressOf<ID3D12Resource> depthStencils, RefPtr<BufferView> dstView, uint32 viewCount);
	void createUnorederdAcsessView(RefAddressOf<ID3D12Resource> unorederdAcsess, RefPtr<BufferView> dstView, uint32 viewCount, const D3D12_BUFFER_UAV& buffer);

	void discardRenderTargetView(const BufferView& bufferView);
	void discardConstantBufferView(const BufferView& bufferView);
	void discardShaderResourceView(const BufferView& bufferView);
	void discardDepthStencilView(const BufferView& bufferView);

	//Direct3Dのデスクリプタヒープをタイプから直接取得
	RefPtr<ID3D12DescriptorHeap> getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);
	RefPtr<DescriptorHeap> getDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);

private:

	DescriptorHeap _rtvHeap;
	DescriptorHeap _dsvHeap;
	DescriptorHeap _cbvSrvHeap;
};