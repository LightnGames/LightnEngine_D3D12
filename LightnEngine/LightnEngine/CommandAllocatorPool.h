#pragma once

#include "stdafx.h"
#include <vector>
#include <queue>

class CommandAllocatorPool {
public:
	CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type);
	~CommandAllocatorPool();

	void create(ID3D12Device* device);
	void shutdown();

	//既に実行完了しているアロケーターを要求
	ID3D12CommandAllocator* requestAllocator(UINT64 completedFenceValue);

	//実行完了したアロケータを返却
	void discardAllocator(UINT64 fenceValue, ID3D12CommandAllocator* allocator);

	//アロケータープールの要素数
	inline size_t size() { return _commandAllocatorPool.size(); }

private:
	const D3D12_COMMAND_LIST_TYPE _commandListType;

	ID3D12Device* _device;
	std::vector<ID3D12CommandAllocator*> _commandAllocatorPool;
	std::queue<std::pair<UINT64, ID3D12CommandAllocator*>> _readyAllocators;
};