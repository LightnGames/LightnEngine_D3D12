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

	//�R�}���h�����ɕK�v�ȃR�}���h���X�g�A�A���P�[�^�[���̃f�[�^���擾
	//�����̃p�C�v���C���X�e�[�g�́A�v���R�}���h���X�g�������̃p�C�v���C���X�e�[�g�݂̂����g�p���Ȃ��Ȃǃh���C�o���œK���ł���ꍇ�Ɏw�肷��B
	//�Ⴆ��Bundle�݂̂̕`����s�����B(https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset)
	CommandListSet requestCommandListSet(RefPtr<ID3D12PipelineState> state = nullptr);

	//�R�}���h���X�g�Z�b�g�̃R�}���h���X�g�ƃR�}���h�A���P�[�^�[��ԋp����
	void discardCommandListSet(const CommandListSet& set);

	//�R�}���h���X�g���R�}���h�L���[�ɓn���Ď��s����B�߂�l�͓n�����R�}���h���X�g�̃t�F���X�l
	UINT64 executeCommandList(RefPtr<ID3D12GraphicsCommandList> commandList);

	//�R�}���h���X�g�Z�b�g�̃t�F���X�l�C���N�������g���s��
	void executeCommandList(CommandListSet& set);

	//���ׂẴR�}���h�L���[����������܂őҋ@
	void waitForIdle();

	RefPtr<CommandQueue> getCommandQueue();
	RefPtr<ID3D12CommandQueue> getDirectQueue() const;

protected:
	D3D12_COMMAND_LIST_TYPE _commandListType;

	CommandListPool _commandListPool;
	CommandAllocatorPool _commandAllocatorPool;
	CommandQueue _commandQueue;
};