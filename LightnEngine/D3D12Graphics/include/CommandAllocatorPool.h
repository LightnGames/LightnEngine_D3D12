#pragma once

#include "stdafx.h"
#include <Utility.h>
#include <queue>

class CommandAllocatorPool {
public:
	CommandAllocatorPool();
	~CommandAllocatorPool();

	void create(RefPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type);
	void shutdown();

	//既に実行完了しているアロケーターを要求
	RefPtr<ID3D12CommandAllocator> requestAllocator(UINT64 completedFenceValue);

	//実行完了したアロケータを返却
	void discardAllocator(UINT64 fenceValue, RefPtr<ID3D12CommandAllocator> allocator);

	//アロケータープールの要素数
	inline size_t size() { return _commandAllocatorPool.size(); }

private:
	D3D12_COMMAND_LIST_TYPE _commandListType;

	RefPtr<ID3D12Device> _device;
	VectorArray<ID3D12CommandAllocator*> _commandAllocatorPool;
	std::queue<std::pair<UINT64, RefPtr<ID3D12CommandAllocator>>> _readyAllocators;
};