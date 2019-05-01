#include "DebugGeometry.h"
#include "GpuResourceManager.h"
#include "CommandContext.h"
#include "SharedMaterial.h"


void DebugLineRender::create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext){
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

	for (uint32 i = 0; i < FrameCount; ++i) {
		_perInstanceData[i].createDirectDynamic(device, commandContext, MAX_GIZMO, sizeof(DebugLineVertex));
	}
}

void DebugLineRender::setupRenderCommand(RenderSettings& settings, uint32 instanceCount){
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;
	_material->setupRenderCommand(settings);
	commandList->IASetVertexBuffers(0, 1, &_perInstanceData[frameIndex]._vertexBufferView);
	commandList->DrawInstanced(2, instanceCount, 0, 0);
}

void DebugLineRender::destroy(){
	DebugRender::destroy();
}

void DebugCubeRender::create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext){
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

	for (uint32 i = 0; i < FrameCount; ++i) {
		_perInstanceData[i].createDirectDynamic(device, commandContext, MAX_GIZMO, sizeof(DebugGeometryVertex));
	}

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

void DebugCubeRender::setupRenderCommand(RenderSettings& settings, uint32 instanceCount){
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;
	_material->setupRenderCommand(settings);

	D3D12_VERTEX_BUFFER_VIEW views[] = { _geometryVertexBuffer._vertexBufferView, _perInstanceData[frameIndex]._vertexBufferView };
	commandList->IASetVertexBuffers(0, 2, views);
	commandList->DrawInstanced(24, instanceCount, 0, 0);
}

void DebugCubeRender::destroy(){
	DebugRender::destroy();
	_geometryVertexBuffer.destroy();
}

DebugRender::~DebugRender(){
}

void DebugRender::writeBufferDara(uint32 frameIndex, void* srcPtr, size_t bufferLength){
	memcpy(_perInstanceData[frameIndex]._dataPtr, srcPtr, bufferLength);
}

void DebugRender::destroy(){
	for (uint32 i = 0; i < FrameCount; ++i) {
		_perInstanceData[i].destroy();
	}
}

void DebugSphereRender::create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext){
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
	materialSettings.name = "GizmoS";
	materialSettings.vertexShaderName = "GizmoGeometry.hlsl";
	materialSettings.pixelShaderName = "GizmoGeometry.hlsl";
	materialSettings.vsTextures = {};
	materialSettings.psTextures = {};
	materialSettings.inputLayouts = inputLayouts;
	materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	manager.createSharedMaterial(device, materialSettings);
	manager.loadSharedMaterial("GizmoS", &_material);

	for (uint32 i = 0; i < FrameCount; ++i) {
		_perInstanceData[i].createDirectDynamic(device, commandContext, MAX_GIZMO, sizeof(DebugGeometryVertex));
	}

	constexpr uint32 circleVertexCount = 16;
	constexpr uint32 circleCount = 3;//X,Y,Z
	constexpr uint32 lineVertexCount = 2;//start,end
	constexpr float radius = 0.5f;
	constexpr float perVertexAngle = radianFromDegree(360) / static_cast<float>(circleVertexCount);

	VectorArray<Vector3> vertices(circleVertexCount * circleCount * lineVertexCount);
	uint32 circleIncriment = 0;

	for (uint32 c = 0; c < circleVertexCount; ++c) {
		const uint32 arrayIndex = (circleIncriment * circleVertexCount + c) * lineVertexCount;
		const float angle = c * perVertexAngle;
		vertices[arrayIndex] = Vector3(cosf(angle), 0, sinf(angle)) * radius;

		const uint32 nextArrayIndex = arrayIndex + 1;
		const float nextAngle = (c + 1) * perVertexAngle;
		vertices[nextArrayIndex] = Vector3(cosf(nextAngle), 0, sinf(nextAngle)) * radius;
	}
	circleIncriment++;

	for (uint32 c = 0; c < circleVertexCount; ++c) {
		const uint32 arrayIndex = (circleIncriment * circleVertexCount + c) * lineVertexCount;
		const float angle = c * perVertexAngle;
		vertices[arrayIndex] = Vector3(0, cosf(angle), sinf(angle)) * radius;

		const uint32 nextArrayIndex = arrayIndex + 1;
		const float nextAngle = (c + 1) * perVertexAngle;
		vertices[nextArrayIndex] = Vector3(0, cosf(nextAngle), sinf(nextAngle)) * radius;
	}
	circleIncriment++;

	for (uint32 c = 0; c < circleVertexCount; ++c) {
		const uint32 arrayIndex = (circleIncriment * circleVertexCount + c) * lineVertexCount;
		const float angle = c * perVertexAngle;
		vertices[arrayIndex] = Vector3(cosf(angle), sinf(angle), 0) * radius;

		const uint32 nextArrayIndex = arrayIndex + 1;
		const float nextAngle = (c + 1) * perVertexAngle;
		vertices[nextArrayIndex] = Vector3(cosf(nextAngle), sinf(nextAngle), 0) * radius;
	}
	circleIncriment++;

	_geometryVertexBuffer.createDirect<Vector3>(device, commandContext, vertices);
}

void DebugSphereRender::setupRenderCommand(RenderSettings& settings, uint32 instanceCount){
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;
	_material->setupRenderCommand(settings);

	D3D12_VERTEX_BUFFER_VIEW views[] = { _geometryVertexBuffer._vertexBufferView, _perInstanceData[frameIndex]._vertexBufferView };
	commandList->IASetVertexBuffers(0, 2, views);
	commandList->DrawInstanced(96, instanceCount, 0, 0);
}

void DebugSphereRender::destroy(){
	DebugRender::destroy();
	_geometryVertexBuffer.destroy();
}

void DebugCapsuleRender::create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext){
	GpuResourceManager& manager = GpuResourceManager::instance();

	VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
		{ "POSITION"      , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
		{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "HEIGHT",         0, DXGI_FORMAT_R32_FLOAT,          1, 80, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	};


	SharedMaterialCreateSettings materialSettings;
	materialSettings.name = "GizmoC";
	materialSettings.vertexShaderName = "DebugDrawCapsule.hlsl";
	materialSettings.pixelShaderName = "DebugDrawCapsule.hlsl";
	materialSettings.vsTextures = {};
	materialSettings.psTextures = {};
	materialSettings.inputLayouts = inputLayouts;
	materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	manager.createSharedMaterial(device, materialSettings);
	manager.loadSharedMaterial("GizmoC", &_material);

	for (uint32 i = 0; i < FrameCount; ++i) {
		_perInstanceData[i].createDirectDynamic(device, commandContext, MAX_GIZMO, sizeof(DebugCapsuleRender));
	}

	constexpr float upperSpahere = 1.0f;
	constexpr float lowerSpahere = 0.0f;
	constexpr uint32 verticalLineCount = 4;
	constexpr uint32 circleVertexCount = 16;
	constexpr uint32 circleCount = 4;//X x 2,Y,Z
	constexpr uint32 lineVertexCount = 2;//start,end
	constexpr float radius = 0.5f;
	constexpr float perVertexAngle = radianFromDegree(360) / static_cast<float>(circleVertexCount);

	VectorArray<Vector4> vertices(circleVertexCount * circleCount * lineVertexCount + verticalLineCount * lineVertexCount);
	uint32 circleIncriment = 0;

	for (uint32 c = 0; c < circleVertexCount; ++c) {
		const uint32 arrayIndex = (circleIncriment * circleVertexCount + c) * lineVertexCount;
		const float angle = c * perVertexAngle;
		vertices[arrayIndex] = Vector4(cosf(angle), 0, sinf(angle), upperSpahere) * radius;

		const uint32 nextArrayIndex = arrayIndex + 1;
		const float nextAngle = (c + 1) * perVertexAngle;
		vertices[nextArrayIndex] = Vector4(cosf(nextAngle), 0, sinf(nextAngle), upperSpahere) * radius;
	}
	circleIncriment++;

	for (uint32 c = 0; c < circleVertexCount; ++c) {
		const uint32 arrayIndex = (circleIncriment * circleVertexCount + c) * lineVertexCount;
		const float angle = c * perVertexAngle;
		vertices[arrayIndex] = Vector4(cosf(angle), 0, sinf(angle), lowerSpahere) * radius;

		const uint32 nextArrayIndex = arrayIndex + 1;
		const float nextAngle = (c + 1) * perVertexAngle;
		vertices[nextArrayIndex] = Vector4(cosf(nextAngle), 0, sinf(nextAngle), lowerSpahere) * radius;
	}
	circleIncriment++;

	for (uint32 c = 0; c < circleVertexCount; ++c) {
		const float upperOrLower = (c >= circleVertexCount / 2) ? lowerSpahere : upperSpahere;
		const uint32 arrayIndex = (circleIncriment * circleVertexCount + c) * lineVertexCount;
		const float angle = c * perVertexAngle;
		vertices[arrayIndex] = Vector4(0, sinf(angle), cosf(angle), upperOrLower) *radius;

		const uint32 nextArrayIndex = arrayIndex + 1;
		const float nextAngle = (c + 1) * perVertexAngle;
		vertices[nextArrayIndex] = Vector4(0, sinf(nextAngle), cosf(nextAngle), upperOrLower) *radius;
	}
	circleIncriment++;

	for (uint32 c = 0; c < circleVertexCount; ++c) {
		const float upperOrLower = (c >= circleVertexCount / 2) ? lowerSpahere : upperSpahere;
		const uint32 arrayIndex = (circleIncriment * circleVertexCount + c) * lineVertexCount;
		const float angle = c * perVertexAngle;
		vertices[arrayIndex] = Vector4(cosf(angle), sinf(angle), 0, upperOrLower) * radius;

		const uint32 nextArrayIndex = arrayIndex + 1;
		const float nextAngle = (c + 1) * perVertexAngle;
		vertices[nextArrayIndex] = Vector4(cosf(nextAngle), sinf(nextAngle), 0, upperOrLower) * radius;
	}
	circleIncriment++;

	for (uint32 c = 0; c < circleVertexCount / verticalLineCount; ++c) {
		const uint32 arrayIndex = (circleIncriment * circleVertexCount + c) * lineVertexCount;
		const float angle = c * verticalLineCount * perVertexAngle;
		vertices[arrayIndex] = Vector4(cosf(angle), 0, sinf(angle), upperSpahere) * radius;

		const uint32 nextArrayIndex = arrayIndex + 1;
		vertices[nextArrayIndex] = Vector4(cosf(angle), 0, sinf(angle), lowerSpahere) * radius;
	}
	circleIncriment++;

	_geometryVertexBuffer.createDirect<Vector4>(device, commandContext, vertices);
}

void DebugCapsuleRender::setupRenderCommand(RenderSettings& settings, uint32 instanceCount){
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;
	_material->setupRenderCommand(settings);

	D3D12_VERTEX_BUFFER_VIEW views[] = { _geometryVertexBuffer._vertexBufferView, _perInstanceData[frameIndex]._vertexBufferView };
	commandList->IASetVertexBuffers(0, 2, views);
	commandList->DrawInstanced(136, instanceCount, 0, 0);
}

void DebugCapsuleRender::destroy(){
	DebugRender::destroy();
	_geometryVertexBuffer.destroy();
}
