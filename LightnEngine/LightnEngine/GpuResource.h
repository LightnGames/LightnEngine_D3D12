#pragma once

#include "stdafx.h"

using namespace Microsoft::WRL;

class GpuResource {
public:
	GpuResource():
		_usageState(D3D12_RESOURCE_STATE_COMMON),
		_transitionState(D3D12_RESOURCE_STATE_UNKNOWN),
		_gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL) {}

	GpuResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES currentState):
		_resource(pResource),
		_usageState(currentState),
		_transitionState(D3D12_RESOURCE_STATE_UNKNOWN),
		_gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL){}

	virtual void destroy() {
		_resource = nullptr;
		_gpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
	}

	ID3D12Resource* operator->() { return _resource.Get(); }
	const ID3D12Resource* operator->() const { return _resource.Get(); }

	ID3D12Resource* getResource() { return _resource.Get(); }
	const ID3D12Resource* getResource() const { return _resource.Get(); }

	D3D12_GPU_VIRTUAL_ADDRESS getGpuVirtualAddress() const { return _gpuVirtualAddress; }

protected:
	ComPtr<ID3D12Resource> _resource;
	D3D12_RESOURCE_STATES _usageState;
	D3D12_RESOURCE_STATES _transitionState;
	D3D12_GPU_VIRTUAL_ADDRESS _gpuVirtualAddress;
};