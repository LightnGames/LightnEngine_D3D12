#pragma once

#include "stdafx.h"
#include <queue>
#include "Utility.h"

class CommandListPool {
public:
	CommandListPool();
	~CommandListPool();

	void create(RefPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type);
	void shutdown();

	//既に実行完了しているコマンドリストを要求
	RefPtr<ID3D12GraphicsCommandList> requestCommandList(UINT64 completedFenceValue, RefPtr<ID3D12CommandAllocator> allocator);

	//実行完了したコマンドリストを返却
	void discardCommandList(UINT64 fenceValue, RefPtr<ID3D12GraphicsCommandList> list);

	//アロケータープールの要素数
	inline size_t getSize() const;

private:
	D3D12_COMMAND_LIST_TYPE _commandListType;

	RefPtr<ID3D12Device> _device;
	VectorArray<ID3D12GraphicsCommandList*> _commandListPool;
	std::queue<std::pair<UINT64, RefPtr<ID3D12GraphicsCommandList>>> _readyCommandLists;
};