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

	//���Ɏ��s�������Ă���R�}���h���X�g��v��
	ID3D12GraphicsCommandList* requestCommandList(UINT64 completedFenceValue, ID3D12CommandAllocator* allocator);

	//���s���������R�}���h���X�g��ԋp
	void discardCommandList(UINT64 fenceValue, ID3D12GraphicsCommandList* list);

	//�A���P�[�^�[�v�[���̗v�f��
	inline size_t size() { return _commandListPool.size(); }

private:
	const D3D12_COMMAND_LIST_TYPE _commandListType;

	ID3D12Device* _device;
	std::vector<ID3D12GraphicsCommandList*> _commandListPool;
	std::queue<std::pair<UINT64, ID3D12GraphicsCommandList*>> _readyCommandLists;
};