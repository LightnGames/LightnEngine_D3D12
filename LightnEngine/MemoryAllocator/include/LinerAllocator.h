#pragma once

#include <iostream>
#include <cassert>

#include <Type.h>

//#define DEBUG_ARRAY
//#define DEBUG_PRINT_ENABLE

#ifdef DEBUG_PRINT_ENABLE
#define DEBUG_PRINT(str) std::cout << str << std::endl;
#else
#define DEBUG_PRINT(str)
#endif

class LinerAllocator {
public:
	LinerAllocator() : offset(0), mainMemorySize(0), mainMemory(nullptr) {
	}

	~LinerAllocator() {
	}

	void init(ulong2 allocateSize = 128) {
		offset = 0;
		mainMemorySize = allocateSize;
		mainMemory = new byte[mainMemorySize];
		memset(mainMemory, 0, mainMemorySize);

		DEBUG_PRINT("allocate: " << mainMemorySize << "byte ");
	}

	void shutdown() {
		delete[] mainMemory;
		offset = 0;
		mainMemorySize = 0;
		mainMemory = nullptr;
		DEBUG_PRINT("shutdown allocator");
	}

	byte* divideMemory(size_t size) {
		assert(offset + size <= mainMemorySize && "‚±‚êˆÈãƒƒ‚ƒŠ‚ðŠm•Ûo—ˆ‚Ü‚¹‚ñ");
		byte * mem = reinterpret_cast<byte*>(mainMemory + offset);
		offset += size;
		DEBUG_PRINT("Divide Ptr: " << offset << " Size: " << size);

		return mem;
	}

	template<class T>
	T * allocate() {
		return new (reinterpret_cast<T*>(divideMemory(sizeof(T)))) T();
	}

	ulong2 offset;
	ulong2 mainMemorySize;
	byte* mainMemory;
};

struct TestStruct {
	uint32 str[9];

	TestStruct() {
		str[0] = 1;
		str[1] = 2;
		str[2] = 3;
		str[3] = 4;
		str[4] = 5;
		str[5] = 6;
		str[6] = UINT32_MAX;
		str[7] = 8;
		str[8] = 9;
	}

	void print() {
		std::cout << "TestStruct Print: ";
		for (int i = 0; i < 9; ++i) {
			std::cout << str[i] << " ";
		}

		std::cout << std::endl;
	}
};

//int main() {
//	LinerAllocator allocator;
//	allocator.init();
//
//	int32* p = allocator.allocate<int32>();
//	TestStruct* p1 = allocator.allocate<TestStruct>();
//	*p = 999;
//
//	std::cout << *p << std::endl;
//	p1->print();
//
//	return 0;
//}