#pragma once

using byte = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;
using ulong = unsigned long;
using ulong2 = unsigned long long;
using int32 = int;

#define GETTER(type, var)\
private:\
	type _##var;\
public:\
	inline type var() { return _##var; }\
	inline type var() const{ return _##var;}\

#define GETSET(type, var)\
	GETTER(type,var)\
public:\
	void var(type value) { _##var = value; }


struct NonCopyable {
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

template <class T>
class Singleton :private NonCopyable {
public:
	Singleton() {
		//assert(!_singleton);
		_singleton = static_cast<T*>(this);
	}

	static T& instance() {
		return *_singleton;
	}

protected:

	static T* _singleton;
};

#include <new>

template <class T>
struct MyAllocator {
	// �v�f�̌^
	using value_type = T;

	// ����֐�
	// (�f�t�H���g�R���X�g���N�^�A�R�s�[�R���X�g���N�^
	//  �A���[�u�R���X�g���N�^)
	MyAllocator() {}

	// �ʂȗv�f�^�̃A���P�[�^���󂯎��R���X�g���N�^
	template <class U>
	MyAllocator(const MyAllocator<U>&) {}

	// �������m��
	T* allocate(std::size_t n) {
		return reinterpret_cast<T*>(std::malloc(sizeof(T) * n));
	}

	// ���������
	void deallocate(T* p, std::size_t n) {
		static_cast<void>(n);
		std::free(p);
	}
};

// ��r���Z�q
template <class T, class U>
bool operator==(const MyAllocator<T>&, const MyAllocator<U>&) {
	return true;
}

template <class T, class U>
bool operator!=(const MyAllocator<T>&, const MyAllocator<U>&) {
	return false;
}

#include <vector>
#include <list>
#include <string>
#include <codecvt> 
#include <memory>
#include <unordered_map>

template <class T>
using VectorArray = std::vector<T, MyAllocator<T>>;

template <class T>
using ListArray = std::list<T, MyAllocator<T>>;

template <class T>
using UniquePtr = std::unique_ptr<T>;

template <class T>
using RefPtr = T *;

template <class T, class ...Args>
UniquePtr<T> makeUnique(Args ...args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}

using String = std::basic_string<char, std::char_traits<char>, MyAllocator<char>>;
using WString = std::basic_string<wchar_t, std::char_traits<wchar_t>, MyAllocator<wchar_t>>;

WString convertWString(const String& srcStr);

namespace std {

	//�n�b�V���v�Z�@https://stackoverflow.com/questions/34597260/stdhash-value-on-char-value-and-not-on-memory-address
#if SIZE_MAX == UINT32_MAX
	constexpr std::size_t FNV_PRIME = 16777619u;
	constexpr std::size_t FNV_OFFSET_BASIS = 2166136261u;
#elif SIZE_MAX == UINT64_MAX
	constexpr std::size_t FNV_PRIME = 1099511628211u;
	constexpr std::size_t FNV_OFFSET_BASIS = 14695981039346656037u;
#else
#error "size_t must be greater than 32 bits."
#endif

	template <>
	struct hash<String> : public unary_function<String, size_t> {
		size_t operator()(const String& value) const {
			std::size_t hash = FNV_OFFSET_BASIS;
			for (auto it = value.begin(); it != value.end(); it++) {
				hash ^= *it;
				hash *= FNV_PRIME;
			}
			return hash;
		}
	};
} // namespace std

template <class T, class U>
using UnorderedMap = std::unordered_map < T, U, std::hash<T>, std::equal_to<T>, MyAllocator<std::pair<const T, U>>>; 

//�ȉ��O���t�B�b�N�X�C���^�[�t�F�[�X��`�B�O���t�B�b�N�XAPI�C���^�[�t�F�[�X�w�b�_�[�Ɉړ�����
struct SharedMaterialCreateSettings {
	String name;
	String vertexShaderName;
	String pixelShaderName;
	VectorArray<String> vsTextures;
	VectorArray<String> psTextures;
};