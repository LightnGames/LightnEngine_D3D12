#include "DescriptorHeap.h"
#include "D3D12Helper.h"
#include "D3D12Util.h"

#include <GenericAllocator.h>

DescriptorHeapManager* Singleton<DescriptorHeapManager>::_singleton = 0;

DescriptorHeap::DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) :_descriptorHeapType(type) {
}


DescriptorHeap::~DescriptorHeap() {
	shutdown();
}

void DescriptorHeap::create(RefPtr<ID3D12Device> device, uint32 maxDescriptorCount) {
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

	_allocator = new GenericAllocator(sizeof(BufferView));
	_allocator->init(maxDescriptorCount);
}

void DescriptorHeap::shutdown() {
	delete _allocator;
	_descriptorHeap = nullptr;
}

void DescriptorHeap::allocateBufferView(RefPtr<BufferView> bufferView, uint32 descriptorCount) {
	void* bufferViewPtr = _allocator->divideMemory(descriptorCount);
	Block* block = _allocator->getBlockFromDataPtr(bufferViewPtr);

	bufferView->location = _allocator->blockLocalIndex(block);
	bufferView->size = block->size;
	bufferView->gpuHandle = gpuHandle(bufferView->location);
	bufferView->cpuHandle = cpuHandle(bufferView->location);
}

void DescriptorHeap::discardBufferView(const BufferView& bufferView) {
	//�f�X�N���v�^�[�̃C���f�b�N�X���烁�����A���P�[�^�[�̊Ǘ�����f�[�^�|�C���^�𕜌�
	void* bufferViewPtr = _allocator->getDataPtrFromBlockLocation(bufferView.location);
	_allocator->releaseMemory(bufferViewPtr);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::gpuHandle(ulong2 bufferViewLocation) const {
	D3D12_GPU_DESCRIPTOR_HANDLE rtvHandle = _gpuHandleStart;
	rtvHandle.ptr += _incrimentSize * static_cast<SIZE_T>(bufferViewLocation);
	return rtvHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::cpuHandle(ulong2 bufferViewLocation) const {
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _cpuHandleStart;
	rtvHandle.ptr += _incrimentSize * static_cast<SIZE_T>(bufferViewLocation);
	return rtvHandle;
}

RefPtr<ID3D12DescriptorHeap> DescriptorHeap::descriptorHeap() const{
	return _descriptorHeap.Get();
}

UINT DescriptorHeap::incrimentSize() const{
	return _incrimentSize;
}

DescriptorHeapManager::DescriptorHeapManager():
	_rtvHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
	_dsvHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
	_cbvSrvHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
}

DescriptorHeapManager::~DescriptorHeapManager() {
}

void DescriptorHeapManager::create(RefPtr<ID3D12Device> device) {
	_rtvHeap.create(device);
	_dsvHeap.create(device);
	_cbvSrvHeap.create(device);
}

void DescriptorHeapManager::shutdown() {
}

void DescriptorHeapManager::createRenderTargetView(ID3D12Resource ** textureBuffers, RefPtr<BufferView> dstView, uint32 viewCount) {
	assert(viewCount > 0 && "Request RenderTarget View 0");
	_rtvHeap.allocateBufferView(dstView, viewCount);

	ID3D12Device* device = nullptr;
	textureBuffers[0]->GetDevice(__uuidof(*device), reinterpret_cast<void**>(&device));

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dstView->cpuHandle;

	D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc = {};
	for (uint32 i = 0; i < viewCount; ++i) {
		auto desc = textureBuffers[i]->GetDesc();
		rtvViewDesc.Format = desc.Format;
		rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvViewDesc.Texture2D.MipSlice = 0;
		rtvViewDesc.Texture2D.PlaneSlice = 0;

		device->CreateRenderTargetView(textureBuffers[i], &rtvViewDesc, rtvHandle);
		rtvHandle.ptr += _rtvHeap.incrimentSize();
		//NAME_D3D12_OBJECT(_renderTarget[i]);
	}

	device->Release();
}

void DescriptorHeapManager::createConstantBufferView(ID3D12Resource ** constantBuffers, RefPtr<BufferView> dstView, uint32 viewCount) {
	assert(viewCount > 0 && "Request ConstantBuffer View 0");
	_cbvSrvHeap.allocateBufferView(dstView, viewCount);

	ID3D12Device* device = nullptr;
	constantBuffers[0]->GetDevice(__uuidof(*device), reinterpret_cast<void**>(&device));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = dstView->cpuHandle;
	D3D12_CONSTANT_BUFFER_VIEW_DESC constantDesc = {};
	for (uint32 i = 0; i < viewCount; ++i) {
		auto desc = constantBuffers[i]->GetDesc();
		constantDesc.BufferLocation = constantBuffers[i]->GetGPUVirtualAddress();
		constantDesc.SizeInBytes = static_cast<UINT>(desc.Width);

		device->CreateConstantBufferView(&constantDesc, descriptorHandle);
		descriptorHandle.ptr += _cbvSrvHeap.incrimentSize();
	}

	device->Release();
}

void DescriptorHeapManager::createShaderResourceView(ID3D12Resource ** shaderResources, RefPtr<BufferView> dstView, uint32 viewCount) {
	assert(viewCount > 0 && "Request ShaderResource View 0");
	_cbvSrvHeap.allocateBufferView(dstView, viewCount);

	ID3D12Device* device = nullptr;
	shaderResources[0]->GetDevice(__uuidof(*device), reinterpret_cast<void**>(&device));

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = dstView->cpuHandle;
	for (uint32 i = 0; i < viewCount; ++i) {
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		auto desc = shaderResources[i]->GetDesc();
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = desc.Format;

		//�e�N�X�`���z�񂪂U������΃L���[�u�}�b�v�Ƃ��ď�������...
		if (desc.DepthOrArraySize == 6) {
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = desc.MipLevels;
		}
		else {
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = desc.MipLevels;
		}

		device->CreateShaderResourceView(shaderResources[i], &srvDesc, descriptorHandle);
		descriptorHandle.ptr += _cbvSrvHeap.incrimentSize();
	}

	device->Release();
}

void DescriptorHeapManager::createDepthStencilView(ID3D12Resource ** depthStencils, RefPtr<BufferView> dstView, uint32 viewCount) {
	assert(viewCount > 0 && "Request DepthStencil View 0");
	_dsvHeap.allocateBufferView(dstView, viewCount);

	ID3D12Device* device = nullptr;
	depthStencils[0]->GetDevice(__uuidof(*device), reinterpret_cast<void**>(&device));

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dstView->cpuHandle;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	for (uint32 i = 0; i < viewCount; ++i) {
		auto desc = depthStencils[i]->GetDesc();
		dsvDesc.Format = desc.Format;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(depthStencils[i], &dsvDesc, dsvHandle);
		dsvHandle.ptr += _dsvHeap.incrimentSize();
	}

	device->Release();
}

void DescriptorHeapManager::discardRenderTargetView(const BufferView& bufferView) {
	_rtvHeap.discardBufferView(bufferView);
}

void DescriptorHeapManager::discardConstantBufferView(const BufferView& bufferView) {
	_cbvSrvHeap.discardBufferView(bufferView);
}

void DescriptorHeapManager::discardShaderResourceView(const BufferView& bufferView) {
	_cbvSrvHeap.discardBufferView(bufferView);
}

void DescriptorHeapManager::discardDepthStencilView(const BufferView& bufferView) {
	_dsvHeap.discardBufferView(bufferView);
}


RefPtr<ID3D12DescriptorHeap> DescriptorHeapManager::getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) {
	switch (type) {
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return _cbvSrvHeap.descriptorHeap(); break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV: return _rtvHeap.descriptorHeap(); break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV: return _dsvHeap.descriptorHeap(); break;
	}

	return nullptr;
}

RefPtr<DescriptorHeap> DescriptorHeapManager::getDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) {
	switch (type) {
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return &_cbvSrvHeap; break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV: return &_rtvHeap; break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV: return &_dsvHeap; break;
	}

	return nullptr;
}
