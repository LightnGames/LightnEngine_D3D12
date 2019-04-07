#pragma once

#include "stdafx.h"
#include "Utility.h"

class CommandAllocatorPool;
class GpuResource;

using namespace Microsoft::WRL;
class CommandQueue :private NonCopyable {
public:
	CommandQueue(D3D12_COMMAND_LIST_TYPE type);
	virtual ~CommandQueue();

	void create(ID3D12Device* device);

	void waitForFence(UINT64 fenceValue);
	void waitForIdle();
	bool isFenceComplete(UINT64 fenceValue);
	UINT64 incrementFence();
	UINT64 executeCommandList(ID3D12CommandList* commandList);

	UINT64 fenceValue() { return _fence->GetCompletedValue(); }

	void shutdown();

private:
	GETSET(CommandAllocatorPool*, commandAllocatorPool);
	GETSET(ID3D12CommandQueue*, commandQueue);
	GETSET(UINT64, nextFenceValue);

	D3D12_COMMAND_LIST_TYPE _commandListType;
	UINT64 _lastFenceValue;
	ID3D12Fence* _fence;
	HANDLE _fenceEvent;
};