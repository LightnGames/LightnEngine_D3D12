#include "CommandQueue.h"
#include "D3D12Helper.h"
#include "CommandAllocatorPool.h"

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type):_commandListType(type) {
}

CommandQueue::~CommandQueue() {
	shutdown();
}

void CommandQueue::create(ID3D12Device * device) {
	//コマンドキュー生成
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = _commandListType;
	throwIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue)));

	//フェンス生成
	throwIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
	_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr) {
		throwIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	_commandAllocatorPool = new CommandAllocatorPool(_commandListType);
	_commandAllocatorPool->create(device);
}

void CommandQueue::waitForFence(UINT64 fenceValue) {
	//引数のフェンス値が最新のフェンス値以下であればすでに終了している
	if (isFenceComplete(fenceValue)) {
		return;
	}

	//フェンスが完了するまで待機
	throwIfFailed(_fence->SetEventOnCompletion(fenceValue, _fenceEvent));
	WaitForSingleObjectEx(_fenceEvent, INFINITE, FALSE);
	_lastFenceValue = fenceValue;
}

//現在のキューが完了するまで待機
void CommandQueue::waitForIdle() {
	auto fenceValue = incrementFence();
	waitForFence(fenceValue);
}

//フェンスが引数以下になっているか？
bool CommandQueue::isFenceComplete(UINT64 fenceValue) {
	if (fenceValue > _lastFenceValue) {
		_lastFenceValue = max(_lastFenceValue, _fence->GetCompletedValue());
	}

	return fenceValue <= _lastFenceValue;
}

//フェンスにシグナルを出してインクリメント
UINT64 CommandQueue::incrementFence() {
	throwIfFailed(_commandQueue->Signal(_fence, _nextFenceValue));
	return _nextFenceValue++;
}

UINT64 CommandQueue::executeCommandList(ID3D12CommandList * commandList) {
	//描画コマンドを積み終えたのでコマンドリストを閉じる
	throwIfFailed(((ID3D12GraphicsCommandList*)commandList)->Close());

	//コマンドキューにコマンドリストを渡して実行
	_commandQueue->ExecuteCommandLists(1, &commandList);

	return incrementFence();
}

void CommandQueue::shutdown() {
	CloseHandle(_fenceEvent);
	_fence->Release();
	_commandQueue->Release();
	_fence = nullptr;
	_commandQueue = nullptr;

	delete _commandAllocatorPool;
	_commandAllocatorPool = nullptr;
}
