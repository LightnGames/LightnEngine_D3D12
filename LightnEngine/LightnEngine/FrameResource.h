#pragma once

#include "stdafx.h"

struct BufferView;
class ConstantBuffer;
class Texture2D;

using namespace Microsoft::WRL;
class FrameResource {
public:
	FrameResource(ID3D12Device* device, IDXGISwapChain3* swapChain, UINT frameResourceIndex);
	~FrameResource();

	UINT64 _fenceValue;

	Texture2D* _renderTarget;
	BufferView* _rtv;
private:
};

