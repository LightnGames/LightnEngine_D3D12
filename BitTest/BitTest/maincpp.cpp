#include <iostream>
#include <bitset>
#include <vector>
#include <list>
#include <cassert>

using byte = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;
using ulong = unsigned long;
using ulong2 = unsigned long long;
using int32 = int;

constexpr uint32 BLOCK_DATA_SIZE = 32;
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

	//このブロックと終端タグにサイズ数を書き込み
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

//basePtrを開始としたブロックのローカルポインタを返す
inline constexpr ulong2 blockLocalIndex(void* blockPtr, void* basePtr) {
	ulong2 localPtr = reinterpret_cast<ulong2>(blockPtr) - reinterpret_cast<ulong2>(basePtr);
	return localPtr / BLOCK_AND_HEADER_SIZE;
}

//basePtrを開始としたデータ本体のローカルポインタを返す
inline constexpr ulong2 dataLocalIndex(void* blockPtr, void* basePtr) {
	ulong2 localPtr = reinterpret_cast<ulong2>(blockPtr) - reinterpret_cast<ulong2>(basePtr);
	return localPtr / BLOCK_DATA_SIZE;
}

class Allocator {
public:
	~Allocator() {
		shutdown();
	}

	void init(uint32 allocateBlockNum) {
		allocateBlockSize = allocateBlockNum * BLOCK_AND_HEADER_SIZE;
		allocateDataSize = allocateBlockNum * BLOCK_DATA_SIZE;

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

		//要求サイズ以下のビットにマスクをかける
		uint32 mask = ~(fillRightBit(divideBit) >> 1);
		uint32 freeBit = freeTable & mask;

		//要求サイズ以上で一番小さいフリーブロックインデックスを求める
		uint32 targetBit = lowerBit(freeBit);
		uint32 targetIndex = fastLog2(targetBit);

		//要求ブロック数よりフリーブロックインデックスが小さかったらメモリが足りない
		assert(divideBit <= targetBit && "No Memory");

		Block* currentBlock = freeList[targetIndex];
		removeFreeList(currentBlock);

		//同じサイズのブロックだった場合
		if (currentBlock->size == blockNum) {
			return dataPtr + blockLocalIndex(currentBlock, blockPtr) * BLOCK_DATA_SIZE;
		}

		//ブロックを要求サイズに分割して返却
		currentBlock->setSize(currentBlock->size - blockNum);

		byte* newBlockPtr = (byte*)currentBlock + currentBlock->size*BLOCK_AND_HEADER_SIZE;
		Block* newBlock = new(newBlockPtr)Block();
		newBlock->setSize(blockNum);
		newBlock->enable = true;

		//分割前ブロックを再登録
		registFreeList(currentBlock);
		debugAddActiveList(newBlock);

		//ブロックインデックスからデータポインタを算出して返す
		return dataPtr + blockLocalIndex(newBlock, blockPtr) * BLOCK_DATA_SIZE;
	}

	void releaseMemory(void* ptr) {
		//データポインタのローカルオフセットを計算してブロックポインタを求める
		ulong2 blockIndex = dataLocalIndex(ptr, dataPtr);
		Block* block = (Block*)(blockPtr + blockIndex * BLOCK_AND_HEADER_SIZE);

		//前後左右のブロックポインタ
		uint32* prevBlockSize = (uint32*)((byte*)block - sizeof(uint32));
		Block* prevBlock = (Block*)((byte*)block - *prevBlockSize*BLOCK_AND_HEADER_SIZE);
		Block* nextBlock = (Block*)((byte*)block + block->size*BLOCK_AND_HEADER_SIZE);

		//配列の０番目なら前のブロックは存在しない
		if (blockIndex == 0) {
			prevBlock = nullptr;
		}

		//配列の最後なら次のブロックは存在しない
		if (blockIndex == (allocateBlockSize / BLOCK_AND_HEADER_SIZE - block->size)) {
			nextBlock = nullptr;
		}

		debugRemoveActiveList(block);

		//前マージ
		if (prevBlock != nullptr) {
			if (!prevBlock->enable) {
				removeFreeList(prevBlock);
				prevBlock->setSize(prevBlock->size + block->size);

				block = prevBlock;
				DEBUG_PRINT("Prev Merge");
			}
		}

		//後マージ
		if (nextBlock != nullptr) {
			if (!nextBlock->enable) {
				removeFreeList(nextBlock);
				block->setSize(block->size + nextBlock->size);
				DEBUG_PRINT("Next Merge");
			}
		}

		//フリーリストに登録
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
			freeTable |= bit;
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
			freeTable &= ~bit;
		}
	}

	template<typename T>
	T* allocateMemory() {
		T* ptr = (T*)divideMemory((sizeof(T) + (BLOCK_DATA_SIZE - 1)) / BLOCK_DATA_SIZE);
		ptr = new (ptr)T();
		return ptr;
	}

	byte* dataPtr;
	byte* blockPtr;
	uint32 allocateBlockSize;
	uint32 allocateDataSize;
	std::vector<Block*> freeList;
	uint32 freeTable;

	std::list<Block*> activeList;
	};

struct TestStruct {
	int num[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	void print() {
		//std::cout << "TestStruct" << std::endl;
		DEBUG_PRINT("TestStruct" << num[0] << "," << num[1] << "," << num[2] << "," << num[3] << "," << num[4] << "," << num[5] << "," << num[6] << "," << num[7]);
		//std::cout << std::endl;
	}
};

#include<chrono>
void Benchmark() {

	constexpr uint32 size = 2000000;
	std::vector<TestStruct*> ptrs(size);
	//Windows New
	std::chrono::system_clock::time_point start, end;
	start = std::chrono::system_clock::now();

	for (int i = 0; i < ptrs.size(); ++i) {
		ptrs[i] = new TestStruct();
	}

	for (int i = 0; i < ptrs.size(); ++i) {
		delete ptrs[i];
	}

	end = std::chrono::system_clock::now();

	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::cout << "Windows New:" << elapsed << "ms" << std::endl;
	ptrs.clear();
	ptrs.resize(size);

	Allocator allocator;
	allocator.init(ptrs.size());
	start = std::chrono::system_clock::now();

	for (int i = 0; i < ptrs.size(); ++i) {
		ptrs[i] = allocator.allocateMemory<TestStruct>();
	}

	for (int i = 0; i < ptrs.size(); ++i) {
		allocator.releaseMemory(ptrs[i]);
	}

	end = std::chrono::system_clock::now();
	elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
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
	//Allocator allocator;
	//allocator.init(128);

	//int* ptr1 = allocator.allocateMemory<int>();
	//int* ptr2 = allocator.allocateMemory<int>();
	//TestStruct* s = allocator.allocateMemory<TestStruct>();
	//allocator.releaseMemory(ptr1);
	//allocator.releaseMemory(ptr2);
	//int* ptr3 = allocator.allocateMemory<int>();

	////*ptr1 = 1;
	////*ptr2 = 2;
	//*ptr3 = 3;

	////DEBUG_PRINT(*ptr1);
	////DEBUG_PRINT(*ptr2);
	//DEBUG_PRINT(*ptr3);

	//allocator.releaseMemory(ptr3);

	//s->print();
	//allocator.releaseMemory(s);

	Benchmark();

	system("pause");
}