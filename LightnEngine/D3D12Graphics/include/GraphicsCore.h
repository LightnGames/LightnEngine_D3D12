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
#include "RenderableEntity.h"
#include "DebugGeometry.h"

#include "RenderCommand.h"
#include <LinerAllocator.h>

#ifdef _DEBUG
#define DEBUG
#endif

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

	void createSingleMeshMaterial(const String& name, const InitSettingsPerSingleMesh& singleMeshMaterialInfo);

	SingleMeshRenderInstance createSingleMeshRenderInstance(const String& name, const VectorArray<InitSettingsPerSingleMesh>& materialInfos);
	StaticMultiMeshRenderInstance createStaticMultiMeshRender(const String& name, const InitSettingsPerStaticMultiMesh& meshDatas);

	RefPtr<GpuResourceManager> getGpuResourceManager();
	RefPtr<DebugGeometryRender> getDebugGeometryRender();

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
	BufferView _dsv;

	CommandContext _graphicsCommandContext;
	CommandContext _computeCommandContext;
	FrameResource _frameResources[FrameCount];
	DescriptorHeapManager _descriptorHeapManager;
	GpuResourceManager _gpuResourceManager;
	DebugGeometryRender _debugGeometryRender;
	ImguiWindow _imguiWindow;

	RefPtr<FrameResource> _currentFrameResource;

	VectorArray<StaticSingleMesh> _singleMeshes;
	VectorArray<StaticMultiMesh> _multiMeshes;

	ConstantBufferFrame _mainCameraConstantBuffer;
};