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

	//�R�}���h�����ɕK�v�ȃR�}���h���X�g�A�A���P�[�^�[���̃f�[�^���擾
	//�����̃p�C�v���C���X�e�[�g�́A�v���R�}���h���X�g�������̃p�C�v���C���X�e�[�g�݂̂����g�p���Ȃ��Ȃǃh���C�o���œK���ł���ꍇ�Ɏw�肷��B
	//�Ⴆ��Bundle�݂̂̕`����s�����B(https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset)
	CommandListSet requestCommandListSet(ID3D12PipelineState* state = nullptr);

	//�R�}���h���X�g�Z�b�g�̃R�}���h���X�g�ƃR�}���h�A���P�[�^�[��ԋp����
	void discardCommandListSet(const CommandListSet& set);

	//�R�}���h���X�g���R�}���h�L���[�ɓn���Ď��s����B�߂�l�͓n�����R�}���h���X�g�̃t�F���X�l
	UINT64 executeCommandList(ID3D12GraphicsCommandList* commandList);

	//�R�}���h���X�g�Z�b�g�̃t�F���X�l�C���N�������g���s��
	void executeCommandList(CommandListSet& set);

	//���ׂẴR�}���h�L���[����������܂őҋ@
	void waitForIdle();

protected:
	const D3D12_COMMAND_LIST_TYPE _commandListType;

	CommandListPool* _commandListPool;
	CommandAllocatorPool* _commandAllocatorPool;
	GETSET(CommandQueue*, commandQueue);
};