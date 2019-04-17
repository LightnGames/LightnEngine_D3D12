#pragma once

#include "stdafx.h"
#include "CommandAllocatorPool.h"
#include <Utility.h>
class GpuResource;

using namespace Microsoft::WRL;
class CommandQueue :private NonCopyable {
public:
	CommandQueue();
	virtual ~CommandQueue();

	void create(RefPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type);

	void waitForFence(UINT64 fenceValue);
	void waitForIdle();
	bool isFenceComplete(UINT64 fenceValue);
	UINT64 incrementFence();
	UINT64 executeCommandList(RefPtr<ID3D12CommandList> commandList);

	UINT64 fenceValue() const;

	void shutdown();

private:
	CommandAllocatorPool _commandAllocatorPool;
	GETSET(ID3D12CommandQueue*, commandQueue);
	GETSET(UINT64, nextFenceValue);

	D3D12_COMMAND_LIST_TYPE _commandListType;
	UINT64 _lastFenceValue;
	ID3D12Fence* _fence;
	HANDLE _fenceEvent;
};