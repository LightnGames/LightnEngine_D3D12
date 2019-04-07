#pragma once

#include "stdafx.h"
#include <vector>
#include <queue>

class CommandListPool {
public:
	CommandListPool(D3D12_COMMAND_LIST_TYPE type);
	~CommandListPool();

	void create(ID3D12Device* device);
	void shutdown();

	//既に実行完了しているコマンドリストを要求
	ID3D12GraphicsCommandList* requestCommandList(UINT64 completedFenceValue, ID3D12CommandAllocator* allocator);

	//実行完了したコマンドリストを返却
	void discardCommandList(UINT64 fenceValue, ID3D12GraphicsCommandList* list);

	//アロケータープールの要素数
	inline size_t size() { return _commandListPool.size(); }

private:
	const D3D12_COMMAND_LIST_TYPE _commandListType;

	ID3D12Device* _device;
	std::vector<ID3D12GraphicsCommandList*> _commandListPool;
	std::queue<std::pair<UINT64, ID3D12GraphicsCommandList*>> _readyCommandLists;
};