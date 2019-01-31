#pragma once

#include <windows.h>
#include <string>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

#define D3D12_RESOURCE_STATE_UNKNOWN ((D3D12_RESOURCE_STATES)-1)

static const UINT FrameCount = 3;
static const int CommandListCount = 3;
static const int CommandListPre = 0;
static const int CommandListMid = 1;
static const int CommandListPost = 2;

using byte = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;
using ulong = unsigned long;
using ulong2 = unsigned long long;
using int32 = int;

struct SceneConstantBuffer {
	float offset[4];
	float dummy[60];
};