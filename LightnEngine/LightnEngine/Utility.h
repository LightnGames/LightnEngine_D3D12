#pragma once

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

template <typename T>
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