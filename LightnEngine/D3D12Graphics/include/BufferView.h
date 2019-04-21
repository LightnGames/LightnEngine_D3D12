#pragma once
#include <d3d12.h>
#include <Utility.h>

//バッファビュー参照用コピー
struct RefBufferView {
	RefBufferView(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) :
		cpuHandle(cpuHandle), gpuHandle(gpuHandle) {}

	inline bool isEnable() const { return !(cpuHandle.ptr == 0 && gpuHandle.ptr == 0); }

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
};

struct BufferView :private NonCopyable {
	BufferView() :cpuHandle(), gpuHandle(), location(0), size(0) {}
	virtual ~BufferView() {}

	inline bool isEnable() const { return !(cpuHandle.ptr == 0 && gpuHandle.ptr == 0 && location == 0 && size == 0); }

	RefBufferView getRefBufferView() const {
		return RefBufferView(cpuHandle, gpuHandle);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	ulong2 location;
	uint32 size;
};