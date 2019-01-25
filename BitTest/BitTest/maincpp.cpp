#include <iostream>
#include <bitset>
#include <vector>
#include <list>

using byte = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;
using int32 = int;

constexpr uint32 BLOCK_SIZE = 32;
constexpr uint32 SIZE_SCALE = 32;

constexpr int tab32[32] = {
	0, 9, 1, 10, 13, 21, 2, 29,
	11, 14, 16, 18, 22, 25, 3, 30,
	8, 12, 20, 28, 15, 17, 24, 7,
	19, 27, 23, 6, 26, 5, 4, 31 };

struct Block {
	Block() :size(0), prev(nullptr), next(nullptr),enable(false) {}
	byte* dataPtr;
	Block* prev;
	Block* next;
	uint16 size;
	bool enable;
};

inline bool isPowOfTwo(uint32 value) {
	return (value & (value - 1)) == 0;
}

inline int32 lowerBit(int32 bit) {
	return bit & -bit;
}

inline uint32 fillRightBit(uint32 bit) {
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


inline uint32 fastLog2(uint32 value) {
	value = fillRightBit(value);
	return tab32[(uint32_t)(value * 0x07C4ACDD) >> 27];
}

class Allocator {
public:
	void init() {
		uint32 allocateSize = 128;
		uint32 allocateIndex = fastLog2(higherBit(allocateSize));
		std::cout << "AlocateBlockNum:" << allocateSize << " FirstIndex:" << allocateIndex << std::endl;

		blocks.resize(allocateIndex + 1);
		freeTable = 1 << allocateIndex;

		Block* block = new Block();
		block->size = allocateSize;

		blocks[allocateIndex].push_back(block);
	}

	byte* divideMemory(uint32 blockNum) {
		uint32 requestBit = higherBit(blockNum);
		std::cout << "Request:" << blockNum << " byte Index:" << requestBit << std::endl;

		uint32 mask = ~(fillRightBit(requestBit) >> 1);
		uint32 freeBit = freeTable & mask;
		std::cout << "Mask     :" << std::bitset<8>(mask) << std::endl;
		std::cout << "Freetable:" << std::bitset<8>(freeTable) << std::endl;
		std::cout << "Result   :" << std::bitset<8>(freeBit) << std::endl;

		//ブロックインデックスを求める
		uint32 targetBit = lowerBit(freeBit);
		uint32 targetIndex = fastLog2(targetBit);
		std::cout << "TargetIndex:" << targetIndex << " Bit:" << std::bitset<8>(targetBit) << std::endl;

		//ブロック切り出し
		Block* currentBlock = blocks[targetIndex].front();
		blocks[targetIndex].pop_front();
		freeTable &= ~higherBit(currentBlock->size);
		std::cout << "RemoveFreeTable:" << std::bitset<8>(freeTable) << std::endl;
		currentBlock->size -= blockNum;

		uint32 currentBlockBit = higherBit(currentBlock->size);
		uint32 currentBlockIndex = fastLog2(currentBlockBit);
		freeTable |= currentBlockBit;
		blocks[currentBlockIndex].push_back(currentBlock);
		std::cout << "RegistFreeTable Old:" << std::bitset<8>(freeTable) << " CurrentIndex:" << currentBlockIndex << " OldSize:" << currentBlock->size << std::endl;

		Block* newBlock = new Block();
		newBlock->size = blockNum;
		newBlock->enable = true;
		uint32 newBlockIndex = fastLog2(requestBit);
		std::cout << "NewBlockIndex:" << newBlockIndex << " Bit:" << std::bitset<8>(requestBit) << std::endl;
		currentBlock->next = newBlock;
		newBlock->prev = currentBlock;
		
		newBlock->dataPtr = (byte*)newBlock;

		return newBlock->dataPtr;
	}

	void releaseMemory(byte* dataPtr) {
		Block* block = (Block*)dataPtr;
		Block* prevBlock = block->prev;
		Block* nextBlock = block->next;

		//前マージ
		if (prevBlock != nullptr) {
			if (!prevBlock->enable) {
				uint32 prevBit = higherBit(prevBlock->size);
				uint32 prebIndex = fastLog2(prevBit);

				blocks[prebIndex].remove(prevBlock);
				freeTable &= ~prevBit;

				prevBlock->size += block->size;
				prevBlock->next = block->next;

				block = prevBlock;
			}
		}


		//後マージ
		if (nextBlock != nullptr) {
			if (!nextBlock->enable) {
				uint32 nextBit = higherBit(nextBlock->size);
				uint32 nextIndex = fastLog2(nextBit);

				blocks[nextIndex].remove(nextBlock);
				freeTable &= ~nextBit;

				block->size += nextBlock->size;
				block->next = nextBlock->next;
			}
		}

		//マージ後ブロックを再登録
		uint32 mergeBit = higherBit(block->size);
		uint32 mergeIndex = fastLog2(mergeBit);
		freeTable |= mergeBit;
		block->enable = false;
		block->dataPtr = nullptr;
		blocks[mergeIndex].push_back(block);
	}

	std::vector<std::list<Block*>> blocks;
	uint32 freeTable;
};

void main() {

	int num = 54;

	std::cout << num << std::endl;
	std::cout << std::bitset<8>(num) << std::endl;

	int msb = higherBit(num);
	std::cout << msb << std::endl;
	std::cout << std::bitset<8>(msb) << std::endl;
	std::cout << "fast log " << fastLog2(msb) << std::endl;

	int lsb = lowerBit(num);
	std::cout << lsb << std::endl;
	std::cout << std::bitset<8>(lsb) << std::endl;

	//=========================================================================
	Allocator allocator;
	allocator.init();

	byte* ptr1 = allocator.divideMemory(34);
	byte* ptr2 = allocator.divideMemory(34);
	byte* ptr3 = allocator.divideMemory(34);

	allocator.releaseMemory(ptr1);
	allocator.releaseMemory(ptr2);
	allocator.releaseMemory(ptr3);

	system("pause");
}