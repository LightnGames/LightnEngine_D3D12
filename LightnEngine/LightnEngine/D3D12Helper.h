#pragma once
#include "stdafx.h"

inline std::string HrToString(HRESULT hr) {
	char str[64] = {};
	printf_s(str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(str);
}

class HrException :public std::runtime_error {
public:
	HrException(HRESULT hr) :std::runtime_error(HrToString(hr)), _hr(hr) {}
	HRESULT error() const { return _hr; }
private:
	const HRESULT _hr;
};

inline void throwIfFailed(HRESULT hr) {
	if (FAILED(hr)) {
		throw HrException(hr);
	}
}

#ifdef DEBUG
inline void SetName(ID3D12Object* pObject, LPCWSTR name) {
	pObject->SetName(name);
}

inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index) {
	WCHAR fullName[50];
	if (swprintf_s(fullName, L"%s[%u]", name, index) > 0) {
		pObject->SetName(fullName);
	}
}
#else
inline void SetName(ID3D12Object* pObject, LPCWSTR name) {}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index) {}
#endif

#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)