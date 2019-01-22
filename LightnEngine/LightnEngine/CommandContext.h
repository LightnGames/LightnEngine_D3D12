#pragma once
#include "stdafx.h"
#include "Utility.h"

class CommandListPool;
class CommandAllocatorPool;
class CommandQueue;

struct CommandListSet {
	CommandListSet(ID3D12GraphicsCommandList* list, ID3D12CommandAllocator* allocator, UINT64 fenceValue) :
		commandList(list), allocator(allocator), fenceValue(fenceValue){ }

	ID3D12GraphicsCommandList* commandList;
	ID3D12CommandAllocator* allocator;
	UINT64 fenceValue;
};

class CommandContext :private NonCopyable{
public:

	CommandContext(D3D12_COMMAND_LIST_TYPE type) :_commandListType(type) {}
	virtual ~CommandContext();

	void create(ID3D12Device* device);
	void shutdown();

	CommandListSet requestCommandListSet(ID3D12PipelineState* state = nullptr);
	void discardCommandListSet(const CommandListSet& set);

	UINT64 executeCommandList(ID3D12GraphicsCommandList* commandList);
	void executeCommandList(CommandListSet& set);

protected:
	const D3D12_COMMAND_LIST_TYPE _commandListType;

	CommandListPool* _commandListPool;
	CommandAllocatorPool* _commandAllocatorPool;
	GETSET(CommandQueue*, commandQueue);
};