#pragma once

#include <Utility.h>
#include "GpuResource.h"

//class SharedMaterial;
//class CommandContext;
//struct RenderSettings;
#include <LMath.h>
#include "GpuResourceManager.h"
#include "CommandContext.h"
#include "SharedMaterial.h"

constexpr uint32 MAX_GIZMO = 256;

class DebugLineRender {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext) {
		GpuResourceManager& manager = GpuResourceManager::instance();

		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
			{ "START_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "END_POSITION"  , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};

		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "GizmoT";
		materialSettings.vertexShaderName = "GizmoLine.hlsl";
		materialSettings.pixelShaderName = "GizmoLine.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = {};
		materialSettings.inputLayouts = inputLayouts;
		materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		manager.createSharedMaterial(device, materialSettings);
		manager.loadSharedMaterial("GizmoT", &_material);

		struct GizmoLineVertex {
			Vector3 startPos;
			Vector3 endPos;
			Color color;
		};

		_perInstanceData.createDirectDynamic(device, commandContext, MAX_GIZMO, sizeof(GizmoLineVertex));

		GizmoLineVertex* mapPtr = reinterpret_cast<GizmoLineVertex*>(_perInstanceData._dataPtr);
		Vector3 offset = Vector3::forward * 5;

		new (mapPtr) GizmoLineVertex{ Vector3(0,0,0) + offset,Vector3(1,0,0) + offset,Color::red };
		mapPtr++;

		new (mapPtr) GizmoLineVertex{ Vector3(0,0,0) + offset,Vector3(0,0,1) + offset,Color::blue };
		mapPtr++;

		new (mapPtr) GizmoLineVertex{ Vector3(0,0,0) + offset,Vector3(0,1,0) + offset,Color::green };
		mapPtr++;
	}

	void setupRenderCommand(RenderSettings& settings) {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		_material->setupRenderCommand(settings);
		commandList->IASetVertexBuffers(0, 1, &_perInstanceData._vertexBufferView);
		commandList->DrawInstanced(2, 3, 0, 0);
	}

	void destroy() {
		_perInstanceData.destroy();
	}

private:
	RefPtr<SharedMaterial> _material;
	VertexBufferDynamic _perInstanceData;
};

class DebugCubeRender {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext) {
		GpuResourceManager& manager = GpuResourceManager::instance();

		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
			{ "POSITION"      , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
			{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};


		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "GizmoB";
		materialSettings.vertexShaderName = "GizmoGeometry.hlsl";
		materialSettings.pixelShaderName = "GizmoGeometry.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = {};
		materialSettings.inputLayouts = inputLayouts;
		materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		manager.createSharedMaterial(device, materialSettings);
		manager.loadSharedMaterial("GizmoB", &_material);

		struct GizmoGeometryVertex {
			Matrix4 mtxWorld;
			Color color;
		};

		_perInstanceData.createDirectDynamic(device, commandContext, MAX_GIZMO, sizeof(GizmoGeometryVertex));

		GizmoGeometryVertex* mapPtr = reinterpret_cast<GizmoGeometryVertex*>(_perInstanceData._dataPtr);
		Vector3 offset = Vector3::forward * 5;

		new (mapPtr) GizmoGeometryVertex{ Matrix4::translateXYZ(Vector3(0,0,0) + offset).transpose(), Color::white };
		mapPtr++;

		new (mapPtr) GizmoGeometryVertex{ Matrix4::translateXYZ(Vector3(1,0,0) + offset).transpose(), Color::red };
		mapPtr++;

		new (mapPtr) GizmoGeometryVertex{ Matrix4::translateXYZ(Vector3(0,0,1) + offset).transpose(), Color::blue };
		mapPtr++;

		new (mapPtr) GizmoGeometryVertex{ Matrix4::translateXYZ(Vector3(0,1,0) + offset).transpose(), Color::green };
		mapPtr++;

		VectorArray<Vector3> vertices = {
			{-0.5f,-0.5f,-0.5f},
			{-0.5f, 0.5f,-0.5f},
			{-0.5f, 0.5f,-0.5f},
			{ 0.5f, 0.5f,-0.5f},
			{ 0.5f, 0.5f,-0.5f},
			{ 0.5f,-0.5f,-0.5f},
			{ 0.5f,-0.5f,-0.5f},
			{-0.5f,-0.5f,-0.5f},

			{-0.5f,-0.5f, 0.5f},
			{-0.5f, 0.5f, 0.5f},
			{-0.5f, 0.5f, 0.5f},
			{ 0.5f, 0.5f, 0.5f},
			{ 0.5f, 0.5f, 0.5f},
			{ 0.5f,-0.5f, 0.5f},
			{ 0.5f,-0.5f, 0.5f},
			{-0.5f,-0.5f, 0.5f},

			{-0.5f,-0.5f, 0.5f},
			{-0.5f,-0.5f,-0.5f},
			{-0.5f, 0.5f, 0.5f},
			{-0.5f, 0.5f,-0.5f},
			{ 0.5f, 0.5f, 0.5f},
			{ 0.5f, 0.5f,-0.5f},
			{ 0.5f,-0.5f, 0.5f},
			{ 0.5f,-0.5f,-0.5f},
		};

		_geometryVertexBuffer.createDirect<Vector3>(device, commandContext, vertices);
	}

	void setupRenderCommand(RenderSettings& settings) {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		_material->setupRenderCommand(settings);

		D3D12_VERTEX_BUFFER_VIEW views[] = { _geometryVertexBuffer._vertexBufferView, _perInstanceData._vertexBufferView };
		commandList->IASetVertexBuffers(0, 2, views);
		commandList->DrawInstanced(24, 4, 0, 0);
	}

	void destroy() {
		_perInstanceData.destroy();
		_geometryVertexBuffer.destroy();
	}

private:
	RefPtr<SharedMaterial> _material;
	VertexBuffer _geometryVertexBuffer;
	VertexBufferDynamic _perInstanceData;
};

class DebugGeometryRender :private NonCopyable{
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext) {
		lineRender.create(device, commandContext);
		cubeRender.create(device, commandContext);
	}

	void setupRenderCommand(RenderSettings& settings) {
		lineRender.setupRenderCommand(settings);
		cubeRender.setupRenderCommand(settings);
	}

	void destroy() {
		lineRender.destroy();
		cubeRender.destroy();
	}

private:
	DebugLineRender lineRender;
	DebugCubeRender cubeRender;
};