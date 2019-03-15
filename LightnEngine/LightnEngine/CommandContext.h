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

	//コマンド発効に必要なコマンドリスト、アロケーター等のデータを取得
	//引数のパイプラインステートは、要求コマンドリストが引数のパイプラインステートのみしか使用しないなどドライバが最適化できる場合に指定する。
	//例えばBundleのみの描画を行う時。(https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset)
	CommandListSet requestCommandListSet(ID3D12PipelineState* state = nullptr);
	void discardCommandListSet(const CommandListSet& set);

	UINT64 executeCommandList(ID3D12GraphicsCommandList* commandList);
	void executeCommandList(CommandListSet& set);

	void waitForIdle();

protected:
	const D3D12_COMMAND_LIST_TYPE _commandListType;

	CommandListPool* _commandListPool;
	CommandAllocatorPool* _commandAllocatorPool;
	GETSET(CommandQueue*, commandQueue);
};