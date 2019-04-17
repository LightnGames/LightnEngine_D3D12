#include "CommandAllocatorPool.h"
#include "D3D12Helper.h"

CommandAllocatorPool::CommandAllocatorPool() :_commandListType(), _device(nullptr) {
}

CommandAllocatorPool::~CommandAllocatorPool() {
	shutdown();
}

void CommandAllocatorPool::create(RefPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type) {
	_commandListType = type;
	_device = device;
}

void CommandAllocatorPool::shutdown() {
	for (size_t i = 0; i < _commandAllocatorPool.size(); ++i) {
		_commandAllocatorPool[i]->Release();
	}

	_commandAllocatorPool.clear();
}

RefPtr<ID3D12CommandAllocator> CommandAllocatorPool::requestAllocator(UINT64 completedFenceValue) {
	RefPtr<ID3D12CommandAllocator> allocator = nullptr;

	//�ԋp�ς݃A���P�[�^�[������΂�������g��
	if (!_readyAllocators.empty()) {
		auto& pair = _readyAllocators.front();

		if (pair.first <= completedFenceValue) {
			allocator = pair.second;
			throwIfFailed(allocator->Reset());
			_readyAllocators.pop();
		}
	}

	//�v�[���������Ȃ����������v����t�F���X�̂��̂��Ȃ���ΐV�����������ĕԂ�
	if (allocator == nullptr) {
		throwIfFailed(_device->CreateCommandAllocator(_commandListType, IID_PPV_ARGS(&allocator)));
		_commandAllocatorPool.push_back(allocator);
	}

	return allocator;
}

void CommandAllocatorPool::discardAllocator(UINT64 fenceValue, RefPtr<ID3D12CommandAllocator> allocator) {
	_readyAllocators.push(std::make_pair(fenceValue, allocator));
}
