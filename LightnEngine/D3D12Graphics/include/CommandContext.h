#pragma once
#include "stdafx.h"
#include "Utility.h"
#include "CommandQueue.h"
#include "CommandListPool.h"
#include "CommandAllocatorPool.h"

class CommandListPool;
class CommandAllocatorPool;
class CommandQueue;

struct CommandListSet {
	CommandListSet(RefPtr<ID3D12GraphicsCommandList> list, RefPtr<ID3D12CommandAllocator> allocator, UINT64 fenceValue) :
		commandList(list), allocator(allocator), fenceValue(fenceValue){ }

	RefPtr<ID3D12GraphicsCommandList> commandList;
	RefPtr<ID3D12CommandAllocator> allocator;
	UINT64 fenceValue;
};

class CommandContext :private NonCopyable{
public:
	CommandContext();
	virtual ~CommandContext();

	void create(RefPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type);
	void shutdown();

	//コマンド発効に必要なコマンドリスト、アロケーター等のデータを取得
	//引数のパイプラインステートは、要求コマンドリストが引数のパイプラインステートのみしか使用しないなどドライバが最適化できる場合に指定する。
	//例えばBundleのみの描画を行う時。(https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset)
	CommandListSet requestCommandListSet(RefPtr<ID3D12PipelineState> state = nullptr);

	//コマンドリストセットのコマンドリストとコマンドアロケーターを返却する
	void discardCommandListSet(const CommandListSet& set);

	//コマンドリストをコマンドキューに渡して実行する。戻り値は渡したコマンドリストのフェンス値
	UINT64 executeCommandList(RefPtr<ID3D12GraphicsCommandList> commandList);

	//コマンドリストセットのフェンス値インクリメントも行う
	void executeCommandList(CommandListSet& set);

	//すべてのコマンドキューが完了するまで待機
	void waitForIdle();

	RefPtr<CommandQueue> getCommandQueue();
	RefPtr<ID3D12CommandQueue> getDirectQueue() const;

protected:
	D3D12_COMMAND_LIST_TYPE _commandListType;

	CommandListPool _commandListPool;
	CommandAllocatorPool _commandAllocatorPool;
	CommandQueue _commandQueue;
};