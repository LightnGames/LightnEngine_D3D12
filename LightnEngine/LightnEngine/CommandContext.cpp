#include "CommandContext.h"
#include "CommandListPool.h"
#include "CommandAllocatorPool.h"
#include "CommandQueue.h"
#include "D3D12Helper.h"

CommandContext::~CommandContext() {
	delete _commandQueue;
	delete _commandAllocatorPool;
	delete _commandListPool;
}

void CommandContext::create(ID3D12Device * device) {
	_commandListPool = new CommandListPool(_commandListType);
	_commandAllocatorPool = new CommandAllocatorPool(_commandListType);
	_commandQueue = new CommandQueue(_commandListType);

	_commandListPool->create(device);
	_commandAllocatorPool->create(device);
	_commandQueue->create(device);
}

void CommandContext::shutdown() {
	delete _commandListPool;
	delete _commandAllocatorPool;
	delete _commandQueue;
}

CommandListSet CommandContext::requestCommandListSet(ID3D12PipelineState* state) {
	UINT64 fenceValue = _commandQueue->fenceValue();
	auto* allocator = _commandAllocatorPool->requestAllocator(fenceValue);
	auto* list = _commandListPool->requestCommandList(fenceValue, allocator);

	throwIfFailed(allocator->Reset());
	throwIfFailed(list->Reset(allocator, state));
	
	return CommandListSet(list, allocator, fenceValue);
}

void CommandContext::discardCommandListSet(const CommandListSet & set) {
	_commandListPool->discardCommandList(set.fenceValue, set.commandList);
	_commandAllocatorPool->discardAllocator(set.fenceValue, set.allocator);
}

UINT64 CommandContext::executeCommandList(ID3D12GraphicsCommandList * commandList) {
	return _commandQueue->executeCommandList(commandList);
}

//コマンドリストセットのフェンス値インクリメントも行う版
void CommandContext::executeCommandList(CommandListSet & set) {
	set.fenceValue = _commandQueue->executeCommandList(set.commandList);
}

void CommandContext::waitForIdle() {
	_commandQueue->waitForIdle();
}
