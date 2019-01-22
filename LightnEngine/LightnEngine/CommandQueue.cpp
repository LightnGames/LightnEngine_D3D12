#include "CommandQueue.h"
#include "D3D12Helper.h"
#include "CommandAllocatorPool.h"

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type):_commandListType(type) {
}

CommandQueue::~CommandQueue() {
	shutdown();
}

void CommandQueue::create(ID3D12Device * device) {
	//�R�}���h�L���[����
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = _commandListType;
	throwIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue)));

	//�t�F���X����
	throwIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
	_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr) {
		throwIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	_commandAllocatorPool = new CommandAllocatorPool(_commandListType);
	_commandAllocatorPool->create(device);
}

void CommandQueue::waitForFence(UINT64 fenceValue) {
	//�����̃t�F���X�l���ŐV�̃t�F���X�l�ȉ��ł���΂��łɏI�����Ă���
	if (isFenceComplete(fenceValue)) {
		return;
	}

	//�t�F���X����������܂őҋ@
	throwIfFailed(_fence->SetEventOnCompletion(fenceValue, _fenceEvent));
	WaitForSingleObjectEx(_fenceEvent, INFINITE, FALSE);
	_lastFenceValue = fenceValue;
}

//���݂̃L���[����������܂őҋ@
void CommandQueue::waitForIdle() {
	auto fenceValue = incrementFence();
	waitForFence(fenceValue);
}

//�t�F���X�������ȉ��ɂȂ��Ă��邩�H
bool CommandQueue::isFenceComplete(UINT64 fenceValue) {
	if (fenceValue > _lastFenceValue) {
		_lastFenceValue = max(_lastFenceValue, _fence->GetCompletedValue());
	}

	return fenceValue <= _lastFenceValue;
}

//�t�F���X�ɃV�O�i�����o���ăC���N�������g
UINT64 CommandQueue::incrementFence() {
	throwIfFailed(_commandQueue->Signal(_fence, _nextFenceValue));
	return _nextFenceValue++;
}

UINT64 CommandQueue::executeCommandList(ID3D12CommandList * commandList) {
	//�`��R�}���h��ςݏI�����̂ŃR�}���h���X�g�����
	throwIfFailed(((ID3D12GraphicsCommandList*)commandList)->Close());

	//�R�}���h�L���[�ɃR�}���h���X�g��n���Ď��s
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
