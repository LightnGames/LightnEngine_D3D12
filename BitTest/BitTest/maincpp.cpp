#include <iostream>
#include <bitset>
#include <vector>
#include <list>
#include <cassert>

using byte = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;
using int32 = int;

constexpr uint32 BLOCK_DATA_SIZE = 32;
constexpr uint32 SIZE_ARRAY_SCALE = 32;

constexpr int tab32[32] = {
	0, 9, 1, 10, 13, 21, 2, 29,
	11, 14, 16, 18, 22, 25, 3, 30,
	8, 12, 20, 28, 15, 17, 24, 7,
	19, 27, 23, 6, 26, 5, 4, 31 };

struct Block {
	Block() :size(0), prev(nullptr), next(nullptr),enable(false) {}

	void regist(Block* block) {
		if (next != nullptr) {
			next->prev = block;
		}
		block->next = next;
		block->prev = this;

		next = block;
	}

	void remove() {
		if (next != nullptr) {
			next->prev = prev;
		}

		if (prev != nullptr) {
			prev->next = next;
		}
	}

	Block* prev;
	Block* next;
	uint32 size;
	bool enable;
};

constexpr uint32 getBlockSize(uint32 size) {
	return sizeof(Block) + BLOCK_DATA_SIZE * size;
}

constexpr byte* getBlockDataPtr(Block* block) {
	return (byte*)block + sizeof(Block);
}


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
	void init(uint32 allocateSize) {
		uint32 allocateIndex = fastLog2(higherBit(allocateSize));
		//std::cout << "AlocateBlockNum:" << allocateSize << " FirstIndex:" << allocateIndex << std::endl;

		blocks.resize(allocateIndex + 1);
		freeTable = 1 << allocateIndex;

		allMemorySize = getBlockSize(allocateSize);
		allMemory = new byte[allMemorySize];

		Block* block = new(allMemory) Block();
		block->size = allocateSize;

		blocks[allocateIndex].push_back(block);
	}

	byte* divideMemory(uint32 blockNum) {
		uint32 requestBit = higherBit(blockNum);
		//std::cout << "Request:" << blockNum << " byte Index:" << requestBit << std::endl;

		uint32 mask = ~(fillRightBit(requestBit) >> 1);
		uint32 freeBit = freeTable & mask;
		//std::cout << "Mask     :" << std::bitset<8>(mask) << std::endl;
		//std::cout << "Freetable:" << std::bitset<8>(freeTable) << std::endl;
		//std::cout << "Result   :" << std::bitset<8>(freeBit) << std::endl;

		//ブロックインデックスを求める
		uint32 targetBit = lowerBit(freeBit);
		uint32 targetIndex = fastLog2(targetBit);
		//std::cout << "TargetIndex:" << targetIndex << " Bit:" << std::bitset<8>(targetBit) << std::endl;

		assert(targetBit >= requestBit && "No Memory");

		//ブロック切り出し
		Block* currentBlock = blocks[targetIndex].front();
		blocks[targetIndex].pop_front();
		freeTable &= ~higherBit(currentBlock->size);
		//std::cout << "RemoveFreeTable:" << std::bitset<8>(freeTable) << std::endl;
		currentBlock->size -= blockNum;

		uint32 currentBlockBit = higherBit(currentBlock->size);
		uint32 currentBlockIndex = fastLog2(currentBlockBit);
		freeTable |= currentBlockBit;
		blocks[currentBlockIndex].push_back(currentBlock);
		//std::cout << "RegistFreeTable Old:" << std::bitset<8>(freeTable) << " CurrentIndex:" << currentBlockIndex << " OldSize:" << currentBlock->size << std::endl;

		Block* newBlock = new((byte*)currentBlock + getBlockSize(currentBlock->size)) Block();
		newBlock->size = blockNum;
		newBlock->enable = true;
		uint32 newBlockIndex = fastLog2(requestBit);
		//std::cout << "NewBlockIndex:" << newBlockIndex << " Bit:" << std::bitset<8>(requestBit) << std::endl;

		currentBlock->regist(newBlock);

		//activeBlocks.push_back(newBlock);

		return getBlockDataPtr(newBlock);
	}

	void releaseMemory(void* dataPtr) {
		Block* block = (Block*)((byte*)dataPtr - sizeof(Block));
		Block* prevBlock = block->prev;
		Block* nextBlock = block->next;

		//activeBlocks.remove(block);

		//前マージ
		if (prevBlock != nullptr) {
			if (!prevBlock->enable) {
				uint32 prevBit = higherBit(prevBlock->size);
				uint32 prebIndex = fastLog2(prevBit);

				blocks[prebIndex].remove(prevBlock);
				freeTable &= ~prevBit;
				//std::cout << "FrontMerge" << std::endl;

				prevBlock->size += block->size;
				block->remove();

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
				//std::cout << "AfterMerge" << std::endl;

				nextBlock->enable = false;
				block->size += nextBlock->size;
				nextBlock->remove();
			}
		}

		//マージ後ブロックを再登録
		uint32 mergeBit = higherBit(block->size);
		uint32 mergeIndex = fastLog2(mergeBit);
		freeTable |= mergeBit;
		block->enable = false;
		blocks[mergeIndex].push_back(block);
	}

	template<typename T>
	T* allocateMemory() {
		T* ptr = (T*)divideMemory(sizeof(T));
		ptr = new (ptr)T();
		return ptr;
	}

	byte* allMemory;
	uint32 allMemorySize;
	std::vector<std::list<Block*>> blocks;
	std::list<Block*> activeBlocks;
	uint32 freeTable;
};

struct TestStruct {
	int num[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	void print() {
		//std::cout << "TestStruct" << std::endl;
		for (int i = 0; i < 10; ++i) {
			//std::cout << num[i] << ",";
		}
		//std::cout << std::endl;
	}
};

void Test() {
	Allocator allocator;
	allocator.init(128);

	//サンドイッチ開放テスト
	//□□□
	//■□□　左開放
	//■□■　右開放
	//■■■　最後に真ん中開放　FrontMerge x 2 と AfterMergeが呼ばれる
	{
		int* ptr1;
		int* ptr2;
		int* ptr3;
		ptr1 = allocator.allocateMemory<int>();
		ptr2 = allocator.allocateMemory<int>();
		ptr3 = allocator.allocateMemory<int>();

		*ptr1 = 10;
		*ptr2 = 20;
		*ptr3 = 30;

		//std::cout << *ptr1 << std::endl;
		//std::cout << *ptr2 << std::endl;
		//std::cout << *ptr3 << std::endl;

		//std::cout << "=============== Sandwitch Test ===============" << std::endl;
		allocator.releaseMemory(ptr1);
		allocator.releaseMemory(ptr3);
		allocator.releaseMemory(ptr2); 
		//std::cout << "=============== Sandwitch Test End ===========" << std::endl;
	}

	TestStruct* s;

	s = allocator.allocateMemory<TestStruct>();
	s->print();
	allocator.releaseMemory(s);
}

#include<chrono>
void Benchmark() {

	constexpr uint32 size = 300000;
	std::vector<TestStruct*> ptrs(size);
	//Windows New
	std::chrono::system_clock::time_point start, end;
	start = std::chrono::system_clock::now();

	for (int i = 0; i < ptrs.size(); ++i) {
		ptrs[i] = new TestStruct();
	}

	end = std::chrono::system_clock::now();

	auto elapsed = std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count();
	std::cout << "Windows New:" << elapsed << "ms" << std::endl;
	ptrs.clear();
	ptrs.resize(size);

	Allocator allocator;
	allocator.init(sizeof(TestStruct)*ptrs.size());
	start = std::chrono::system_clock::now();

	for (int i = 0; i < ptrs.size(); ++i) {
		ptrs[i] = allocator.allocateMemory<TestStruct>();
	}

	end = std::chrono::system_clock::now();
	elapsed = std::chrono::duration_cast< std::chrono::milliseconds >(end - start).count();
	std::cout << "Alloctor New:" << elapsed << "ms" << std::endl;
}

void main() {

	int num = 54;

	//std::cout << num << std::endl;
	//std::cout << std::bitset<8>(num) << std::endl;

	int msb = higherBit(num);
	//std::cout << msb << std::endl;
	//std::cout << std::bitset<8>(msb) << std::endl;
	//std::cout << "fast log " << fastLog2(msb) << std::endl;

	int lsb = lowerBit(num);
	//std::cout << lsb << std::endl;
	//std::cout << std::bitset<8>(lsb) << std::endl;

	//=========================================================================
	Test();
	Benchmark();

	system("pause");
}