#pragma once

#include "stdafx.h"

struct BufferView;

using namespace Microsoft::WRL;
class FrameResource {
public:
	FrameResource(ID3D12Device* device, IDXGISwapChain3* swapChain, UINT frameResourceIndex);
	~FrameResource();

	void writeConstantBuffer(const SceneConstantBuffer& buffer);
	ComPtr<ID3D12PipelineState> _pipelineState;

	UINT64 _fenceValue;

	ComPtr<ID3D12Resource> _constantBuffer;
	ComPtr<ID3D12Resource> _renderTarget;

	SceneConstantBuffer* _mappedConstantBufferData;
	BufferView* _sceneCbv;
	BufferView* _rtv;
private:
};

