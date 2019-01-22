#include "CommandListPool.h"
#include "D3D12Helper.h"

CommandListPool::CommandListPool(D3D12_COMMAND_LIST_TYPE type) :_commandListType(type), _device(nullptr) {
}

CommandListPool::~CommandListPool() {
	shutdown();
}

void CommandListPool::create(ID3D12Device * device) {
	_device = device;
}

void CommandListPool::shutdown() {
	for (size_t i = 0; i < _commandListPool.size(); ++i) {
		_commandListPool[i]->Release();
	}

	_commandListPool.clear();
}

ID3D12GraphicsCommandList * CommandListPool::requestCommandList(UINT64 completedFenceValue, ID3D12CommandAllocator* allocator) {
	ID3D12GraphicsCommandList* list = nullptr;

	//返却済みコマンドリストがあればそちらを使う
	if (!_readyCommandLists.empty()) {
		auto& pair = _readyCommandLists.front();

		if (pair.first <= completedFenceValue) {
			list = pair.second;
			_readyCommandLists.pop();
		}
	}

	//プールが何もなかったか合致するフェンスのものがなければ新しく生成して返す
	if (list == nullptr) {
		throwIfFailed(_device->CreateCommandList(0, _commandListType, allocator, nullptr, IID_PPV_ARGS(&list)));
		throwIfFailed(list->Close());
		_commandListPool.push_back(list);
	}

	return list;
}

void CommandListPool::discardCommandList(UINT64 fenceValue, ID3D12GraphicsCommandList * list) {
	_readyCommandLists.push(std::make_pair(fenceValue, list));
}
