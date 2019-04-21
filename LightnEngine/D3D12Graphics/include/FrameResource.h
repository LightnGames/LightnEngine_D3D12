#pragma once

#include "stdafx.h"
#include "BufferView.h"
#include <Utility.h>

struct BufferView;
class ConstantBuffer;
class Texture2D;

using namespace Microsoft::WRL;
class FrameResource {
public:
	FrameResource();
	~FrameResource();

	void create(RefPtr<ID3D12Device> device, RefPtr<IDXGISwapChain3> swapChain, UINT frameResourceIndex);
	void shutdown();

	UINT64 _fenceValue;

	Texture2D* _renderTarget;
	BufferView _rtv;
};

