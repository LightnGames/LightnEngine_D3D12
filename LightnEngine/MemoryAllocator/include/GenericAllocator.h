#pragma once

#pragma once

#include <iostream>
#include <bitset>
#include <vector>
#include <list>
#include <cassert>

#include <Type.h>

constexpr uint32 SIZE_ARRAY_SCALE = 32;

//#define DEBUG_ARRAY
//#define DEBUG_PRINT_ENABLE

#ifdef DEBUG_PRINT_ENABLE
#define DEBUG_PRINT(str) std::cout << str << std::endl;
#else
#define DEBUG_PRINT(str)
#endif

constexpr int tab32[32] = {
	0, 9, 1, 10, 13, 21, 2, 29,
	11, 14, 16, 18, 22, 25, 3, 30,
	8, 12, 20, 28, 15, 17, 24, 7,
	19, 27, 23, 6, 26, 5, 4, 31 };


inline constexpr bool isPowOfTwo(uint32 value) {
	return (value & (value - 1)) == 0;
}

inline constexpr int32 lowerBit(int32 bit) {
	return bit & -bit;
}

inline constexpr uint32 fillRightBit(uint32 bit) {
	bit |= bit >> 1;
	bit |= bit >> 2;
	bit |= bit >> 4;
	bit |= bit >> 8;
	bit |= bit >> 16;
	return bit;
}

inline uint32 higherBit(uint32 bit) {
	uint32 result = bit;
	result--;
	result = fillRightBit(result);
	result++;

	if (!isPowOfTwo(bit)) {
		result >>= 1;
	}

	return result;
}


inline constexpr uint32 fastLog2(uint32 value) {
	value = fillRightBit(value);

	return tab32[(uint32_t)(value * 0x07C4ACDD) >> 27];
}

inline constexpr uint32 fassss(uint32 value) {
	if (isPowOfTwo(value)) {
		return fastLog2(value);
	}
}

struct Block {
	Block() :size(0), listPrev(nullptr), listNext(nullptr), enable(false) {}

	void regist(Block* block) {
		if (listNext != nullptr) {
			listNext->listPrev = block;
		}
		block->listNext = listNext;
		block->listPrev = this;

		listNext = block;
	}

	void remove() {
		if (listNext != nullptr) {
			listNext->listPrev = listPrev;
		}

		if (listPrev != nullptr) {
			listPrev->listNext = listNext;
		}
	}

	//���̃u���b�N�ƏI�[�^�O�ɃT�C�Y������������
	void setSize(uint32 size) {
		this->size = size;
		uint32* endHeaderPtr = (uint32*)((byte*)this + (sizeof(Block) + sizeof(uint32))*size);
		endHeaderPtr -= 1;
		*endHeaderPtr = size;
	}

	Block* listPrev;
	Block* listNext;
	uint32 size;
	bool enable;
};

constexpr uint32 BLOCK_AND_HEADER_SIZE = sizeof(Block) + sizeof(uint32);

class GenericAllocator {
public:
	GenericAllocator(uint32 blockDataSize) :BLOCK_DATA_SIZE(blockDataSize) {}

	~GenericAllocator() {
		shutdown();
	}

	void init(uint32 allocateBlockNum) {
		allocateBlockSize = allocateBlockNum * (ulong2)BLOCK_AND_HEADER_SIZE;
		allocateDataSize = allocateBlockNum * (ulong2)BLOCK_DATA_SIZE;

		dataPtr = new byte[allocateDataSize];
		blockPtr = new byte[allocateBlockSize];

		memset(dataPtr, 0, allocateDataSize);
		memset(blockPtr, 0, allocateBlockSize);

		freeList.resize(SIZE_ARRAY_SCALE);

		Block* block = new(blockPtr)Block();
		block->size = allocateBlockNum;
		registFreeList(block);
	}

	void shutdown() {
		delete[] dataPtr;
		delete[] blockPtr;
	}

	void* divideMemory(uint32 blockNum) {
		uint32 divideBit = higherBit(blockNum);
		//uint32 divideIndex = fastLog2(divideBit);
		//DEBUG_PRINT(std::bitset<8>(divideBit) << " Index:" << divideIndex);

		//�v���T�C�Y�ȉ��̃r�b�g�Ƀ}�X�N��������
		uint32 mask = ~(fillRightBit(divideBit) >> 1);
		uint32 freeBit = freeListFlags & mask;

		//�v���T�C�Y�ȏ�ň�ԏ������t���[�u���b�N�C���f�b�N�X�����߂�
		uint32 targetBit = lowerBit(freeBit);
		uint32 targetIndex = fastLog2(targetBit);

		//�v���u���b�N�����t���[�u���b�N�C���f�b�N�X�������������烁����������Ȃ�
		assert(divideBit <= targetBit && "No Memory");

		Block* currentBlock = freeList[targetIndex];
		removeFreeList(currentBlock);

		//�����T�C�Y�̃u���b�N�������ꍇ
		if (currentBlock->size == blockNum) {
			currentBlock->enable = true;
			return dataPtr + blockLocalIndex(currentBlock) * BLOCK_DATA_SIZE;
		}

		//�u���b�N��v���T�C�Y�ɕ������ĕԋp
		currentBlock->setSize(currentBlock->size - blockNum);

		byte* newBlockPtr = (byte*)currentBlock + currentBlock->size*BLOCK_AND_HEADER_SIZE;
		Block* newBlock = new(newBlockPtr)Block();
		newBlock->setSize(blockNum);
		newBlock->enable = true;

		//�����O�u���b�N���ēo�^
		registFreeList(currentBlock);
		debugAddActiveList(newBlock);

		//�u���b�N�C���f�b�N�X����f�[�^�|�C���^���Z�o���ĕԂ�
		return dataPtr + blockLocalIndex(newBlock) * BLOCK_DATA_SIZE;
	}

	void releaseMemory(void* dataPtr) {
		//�f�[�^�|�C���^�̃��[�J���I�t�Z�b�g���v�Z���ău���b�N�|�C���^�����߂�
		ulong2 blockIndex = dataLocalIndex(dataPtr);
		Block* block = (Block*)(blockPtr + blockIndex * BLOCK_AND_HEADER_SIZE);

		//�O�㍶�E�̃u���b�N�|�C���^
		uint32* prevBlockSize = (uint32*)((byte*)block - sizeof(uint32));
		Block* prevBlock = (Block*)((byte*)block - *prevBlockSize*BLOCK_AND_HEADER_SIZE);
		Block* nextBlock = (Block*)((byte*)block + block->size*BLOCK_AND_HEADER_SIZE);

		//�z��̂O�ԖڂȂ�O�̃u���b�N�͑��݂��Ȃ�
		if (blockIndex == 0) {
			prevBlock = nullptr;
		}

		//�z��̍Ō�Ȃ玟�̃u���b�N�͑��݂��Ȃ�
		if (blockIndex == (allocateBlockSize / BLOCK_AND_HEADER_SIZE - block->size)) {
			nextBlock = nullptr;
		}

		debugRemoveActiveList(block);

		//�O�}�[�W
		if (prevBlock != nullptr) {
			if (!prevBlock->enable) {
				removeFreeList(prevBlock);
				prevBlock->setSize(prevBlock->size + block->size);

				block = prevBlock;
				DEBUG_PRINT("Prev Merge");
			}
		}

		//��}�[�W
		if (nextBlock != nullptr) {
			if (!nextBlock->enable) {
				removeFreeList(nextBlock);
				block->setSize(block->size + nextBlock->size);
				DEBUG_PRINT("Next Merge");
			}
		}

		//�t���[���X�g�ɓo�^
		block->enable = false;
		registFreeList(block);
	}

#ifdef DEBUG_ARRAY
	void debugAddActiveList(Block* block) {
		activeList.push_back(block);
	}

	void debugRemoveActiveList(Block* block) {
		activeList.remove(block);
	}
#else
	inline void debugAddActiveList(Block* block) {}
	inline void debugRemoveActiveList(Block* block) {}
#endif

	void registFreeList(Block* block) {
		uint32 bit = higherBit(block->size);
		uint32 index = fastLog2(bit);

		if (freeList[index] == nullptr) {
			freeList[index] = block;
			freeListFlags |= bit;
		}
		else {
			freeList[index]->regist(block);
		}

	}

	void removeFreeList(Block* block) {
		uint32 bit = higherBit(block->size);
		uint32 index = fastLog2(bit);

		block->remove();

		if (freeList[index] == block) {
			freeList[index] = block->listNext;
		}

		if (freeList[index] == nullptr) {
			freeListFlags &= ~bit;
		}
	}

	//�f�[�^�|�C���^����u���b�N�w�b�_�[���擾����
	inline Block* getBlockFromDataPtr(void* dataPtr) {
		return (Block*)(blockPtr + dataLocalIndex(dataPtr) * BLOCK_AND_HEADER_SIZE);
	}

	//basePtr���J�n�Ƃ����f�[�^�{�̂̃��[�J���|�C���^��Ԃ�
	inline ulong2 dataLocalIndex(void* blockPtr) {
		ulong2 localPtr = reinterpret_cast<ulong2>(blockPtr) - reinterpret_cast<ulong2>(dataPtr);
		return localPtr / BLOCK_DATA_SIZE;
	}

	//basePtr���J�n�Ƃ����u���b�N�̃��[�J���|�C���^��Ԃ�
	inline ulong2 blockLocalIndex(void* blockPtr) {
		ulong2 localPtr = reinterpret_cast<ulong2>(blockPtr) - reinterpret_cast<ulong2>(this->blockPtr);
		return localPtr / BLOCK_AND_HEADER_SIZE;
	}

	//�u���b�N���[�J���C���f�b�N�X����A���P�[�^�[�̃f�[�^�|�C���^�����߂�
	void* getDataPtrFromBlockLocation(ulong2 blockLocation) {
		ulong2 localPtr = blockLocation * BLOCK_DATA_SIZE;
		return dataPtr + localPtr;
	}

	template<typename T>
	T* allocateMemory() {
		return (T*)allocateMemory(sizeof(T));//�R���X�g���N�^�̌Ăяo���͕ʓr�K�v�I
	}

	void* allocateMemory(uint32 size) {
		return divideMemory((size + (BLOCK_DATA_SIZE - 1)) / BLOCK_DATA_SIZE);;
	}

	const uint32 BLOCK_DATA_SIZE;
	byte* dataPtr;
	byte* blockPtr;
	ulong2 allocateBlockSize;
	ulong2 allocateDataSize;
	uint32 freeListFlags;
	std::vector<Block*> freeList;

#ifdef DEBUG_ARRAY
	std::list<Block*> activeList;
#endif
};