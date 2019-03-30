#pragma once
#include "Utility.h"
#include "stdafx.h"
#include "GraphicsConstantSettings.h"

#define DEBUG

using namespace Microsoft::WRL;

struct ID3D12Device;
struct IDXGISwapChain3;
struct BufferView;
class DescriptorHeapManager;
class GpuResourceManager;
class ImguiWindow;
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

	UniquePtr<Texture2D> _depthStencil;
	RefPtr<BufferView> _dsv;

	UniquePtr<CommandContext> _commandContext;
	UniquePtr<FrameResource> _frameResources[FrameCount];
	UniquePtr<DescriptorHeapManager> _descriptorHeapManager;
	UniquePtr<GpuResourceManager> _gpuResourceManager;
	UniquePtr<ImguiWindow> _imguiWindow;

	RefPtr<FrameResource> _currentFrameResource;
	RefPtr<MeshRenderSet> _sky;
	RefPtr<MeshRenderSet> _mesh;
};

