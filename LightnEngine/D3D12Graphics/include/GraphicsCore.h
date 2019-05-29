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

#include "StaticMultiMesh.h"
#include <LinerAllocator.h>

#ifdef _DEBUG
#define DEBUG
#endif

using namespace Microsoft::WRL;

struct RenderPassSet {
	RefPtr<SingleMeshRenderMaterial> mainPass;
	RefPtr<SingleMeshRenderMaterial> depthPass;
};

class StaticSingleMesh {
public:
	//void create(const String& name, VectorArray<String>& materialNames) {
	//	GpuResourceManager& manager = GpuResourceManager::instance();
	//	manager.loadVertexAndIndexBuffer(name, &_mesh);

	//	_materials.resize(_mesh->materialDrawRanges.size());
	//	for (size_t i = 0; i < _materials.size(); ++i) {

	//	}
	//}

	void setupRenderCommand(RenderSettings& settings) const {
		for (size_t i = 0; i < _mesh->materialDrawRanges.size(); ++i) {
			_materials[i]->setupRenderCommand(settings, _mtxWorld, _mesh->getRefVertexAndIndexBuffer(i));
		}
	}

	Matrix4 _mtxWorld;
	VectorArray<RefPtr<SingleMeshRenderMaterial>> _materials;
	RefPtr<VertexAndIndexBuffer> _mesh;
};

class StaticMultiMesh {
public:
	void setupCommand(RenderSettings& settings) {
		for (auto&& material : _materials) {
			material->setupRenderCommand(settings);
		}
	}

	VectorArray<RefPtr<StaticMultiMeshMaterial>> _materials;
};

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

	SingleMeshRenderInstance createSingleMeshRenderInstance(const String& name, const VectorArray<String>& materialNames);
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
	UnorderedMap<String, SingleMeshRenderMaterial> _singleMeshRenderMaterials;
	UnorderedMap<String, StaticMultiMeshMaterial> _multiMeshRenderMaterials;

	VectorArray<StaticSingleMesh> _singleMeshes;
	VectorArray<StaticMultiMesh> _multiMeshes;

	ConstantBufferFrame _mainCameraConstantBuffer;
};