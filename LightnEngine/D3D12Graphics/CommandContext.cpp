#include "CommandContext.h"
#include "CommandListPool.h"
#include "CommandAllocatorPool.h"
#include "CommandQueue.h"
#include "D3D12Helper.h"

CommandContext::CommandContext():_commandListType(){
}

CommandContext::~CommandContext() {
}

void CommandContext::create(RefPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type) {
	_commandListType = type;

	_commandListPool.create(device, _commandListType);
	_commandAllocatorPool.create(device, _commandListType);
	_commandQueue.create(device, _commandListType);
}

void CommandContext::shutdown() {
}

CommandListSet CommandContext::requestCommandListSet(RefPtr<ID3D12PipelineState> state) {
	UINT64 fenceValue = _commandQueue.fenceValue();
	auto allocator = _commandAllocatorPool.requestAllocator(fenceValue);
	auto list = _commandListPool.requestCommandList(fenceValue, allocator);

	throwIfFailed(allocator->Reset());
	throwIfFailed(list->Reset(allocator, state));
	
	return CommandListSet(list, allocator, fenceValue);
}

void CommandContext::discardCommandListSet(const CommandListSet & set) {
	_commandListPool.discardCommandList(set.fenceValue, set.commandList);
	_commandAllocatorPool.discardAllocator(set.fenceValue, set.allocator);
}

UINT64 CommandContext::executeCommandList(RefPtr<ID3D12GraphicsCommandList> commandList) {
	return _commandQueue.executeCommandList(commandList);
}

void CommandContext::executeCommandList(CommandListSet & set) {
	set.fenceValue = _commandQueue.executeCommandList(set.commandList);
}

void CommandContext::waitForIdle() {
	_commandQueue.waitForIdle();
}

RefPtr<CommandQueue> CommandContext::getCommandQueue(){
	return &_commandQueue;
}

RefPtr<ID3D12CommandQueue> CommandContext::getDirectQueue() const{
	return _commandQueue.commandQueue();
}
