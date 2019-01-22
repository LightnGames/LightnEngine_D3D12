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