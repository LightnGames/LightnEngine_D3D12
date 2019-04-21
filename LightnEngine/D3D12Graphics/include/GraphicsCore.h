#pragma once
#include "Utility.h"
#include "stdafx.h"
#include "GraphicsConstantSettings.h"

#include "GpuResourceManager.h"
#include "GpuResource.h"
#include "DescriptorHeap.h"
#include "FrameResource.h"
#include "CommandContext.h"
#include "ImguiWindow.h"

#define DEBUG

using namespace Microsoft::WRL;

class GraphicsCore :private NonCopyable {
public:
	GraphicsCore();
	~GraphicsCore();

	void onInit(HWND hwnd);
	void onUpdate();
	void onRender();
	void onDestroy();

	void createTextures(const VectorArray<String>& textureNames);
	void createMeshSets(const VectorArray<String>& fileNames);
	void createSharedMaterial(const SharedMaterialCreateSettings& settings);

	RefPtr<GpuResourceManager> getGpuResourceManager();

public:
	UINT _width;
	UINT _height;

private:

	//éüÇÃÉtÉåÅ[ÉÄÇ‹Ç≈ë“ã@
	void moveToNextFrame();

	UINT _frameIndex;

	D3D12_VIEWPORT _viewPort;
	D3D12_RECT _scissorRect;

	ComPtr<IDXGISwapChain3> _swapChain;
	ComPtr<ID3D12Device> _device;

	Texture2D _depthStencil;
	RefPtr<BufferView> _dsv;

	CommandContext _commandContext;
	FrameResource _frameResources[FrameCount];
	DescriptorHeapManager _descriptorHeapManager;
	GpuResourceManager _gpuResourceManager;
	ImguiWindow _imguiWindow;

	RefPtr<FrameResource> _currentFrameResource;
};

