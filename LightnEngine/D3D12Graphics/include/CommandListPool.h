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

	//���Ɏ��s�������Ă���R�}���h���X�g��v��
	RefPtr<ID3D12GraphicsCommandList> requestCommandList(UINT64 completedFenceValue, RefPtr<ID3D12CommandAllocator> allocator);

	//���s���������R�}���h���X�g��ԋp
	void discardCommandList(UINT64 fenceValue, RefPtr<ID3D12GraphicsCommandList> list);

	//�A���P�[�^�[�v�[���̗v�f��
	inline size_t getSize() const;

private:
	D3D12_COMMAND_LIST_TYPE _commandListType;

	RefPtr<ID3D12Device> _device;
	VectorArray<ID3D12GraphicsCommandList*> _commandListPool;
	std::queue<std::pair<UINT64, RefPtr<ID3D12GraphicsCommandList>>> _readyCommandLists;
};