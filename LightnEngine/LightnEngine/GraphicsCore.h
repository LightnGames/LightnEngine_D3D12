#pragma once
#include "Utility.h"
#include "stdafx.h"

#define DEBUG

using namespace Microsoft::WRL;

struct BufferView;
class DescriptorHeapManager;
class GpuResourceManager;
class FrameResource;
class CommandQueue;
class CommandContext;
class Texture2D;
class MeshRenderSet;

class GraphicsCore {
public:
	GraphicsCore();
	~GraphicsCore();

	void onInit(HWND hwnd);
	void onUpdate();
	void onRender();
	void onDestroy();

public:
	GETTER(UINT, width);
	GETTER(UINT, height);

private:

	//éüÇÃÉtÉåÅ[ÉÄÇ‹Ç≈ë“ã@
	void moveToNextFrame();

	UINT _frameIndex;

	D3D12_VIEWPORT _viewPort;
	D3D12_RECT _scissorRect;

	ComPtr<IDXGISwapChain3> _swapChain;
	ComPtr<ID3D12Device> _device;

	Texture2D* _depthStencil;
	BufferView* _dsv;

	CommandContext* _commandContext;
	FrameResource* _frameResources[FrameCount];
	FrameResource* _currentFrameResource;
	DescriptorHeapManager* _descriptorHeapManager;
	GpuResourceManager* _gpuResourceManager;

	RefPtr<MeshRenderSet> _mesh;
};

