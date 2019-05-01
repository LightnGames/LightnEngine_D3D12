#pragma once

#include <Utility.h>
#include <LMath.h>
#include "GraphicsConstantSettings.h"
#include "GpuResource.h"

class SharedMaterial;
class CommandContext;
struct RenderSettings;

constexpr uint32 MAX_GIZMO = 256;

struct DebugLineVertex {
	DebugLineVertex(const Vector3& startPos, const Vector3& endPos, const Color& color) :
		startPos(startPos), endPos(endPos), color(color) {}

	Vector3 startPos;
	Vector3 endPos;
	Color color;
};

struct DebugGeometryVertex {
	DebugGeometryVertex(const Matrix4& mtxWorld, const Color& color) :
		mtxWorld(mtxWorld), color(color){}

	Matrix4 mtxWorld;
	Color color;
};

//デバッグ描画基底クラス　バックバッファごとの動的頂点バッファを持っている
class DebugRender :private NonCopyable {
public:
	//インスタンスバッファにデータを書き込み
	void writeBufferDara(uint32 frameIndex, void* srcPtr, size_t bufferLength);
	void destroy();

protected:
	VertexBufferDynamic _perInstanceData[FrameCount];
};

//デバッグワイヤーライン描画クラス
class DebugLineRender :public DebugRender{
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount);
	void destroy();

private:
	RefPtr<SharedMaterial> _material;
};

//デバッグワイヤーキューブ描画クラス
class DebugCubeRender :public DebugRender{
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount);
	void destroy();

private:
	RefPtr<SharedMaterial> _material;
	VertexBuffer _geometryVertexBuffer;
};

//デバッグワイヤースフィア描画クラス
class DebugSphereRender :public DebugRender {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount);
	void destroy();

private:
	RefPtr<SharedMaterial> _material;
	VertexBuffer _geometryVertexBuffer;
};

class DebugGeometryRender :private NonCopyable {
public:
	DebugGeometryRender() {
		_lineDatas.reserve(MAX_GIZMO);
		_cubeDatas.reserve(MAX_GIZMO);
		_sphereDatas.reserve(MAX_GIZMO);
	}

	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext) {
		_lineRender.create(device, commandContext);
		_cubeRender.create(device, commandContext);
		_sphereRender.create(device, commandContext);
	}

	void updatePerInstanceData(uint32 frameIndex) {
		_lineRender.writeBufferDara(frameIndex, _lineDatas.data(), _lineDatas.size() * sizeof(DebugLineVertex));
		_cubeRender.writeBufferDara(frameIndex, _cubeDatas.data(), _cubeDatas.size() * sizeof(DebugGeometryVertex));
		_sphereRender.writeBufferDara(frameIndex, _sphereDatas.data(), _sphereDatas.size() * sizeof(DebugGeometryVertex));
	}

	void setupRenderCommand(RenderSettings& settings) {
		_lineRender.setupRenderCommand(settings, static_cast<uint32>(_lineDatas.size()));
		_cubeRender.setupRenderCommand(settings, static_cast<uint32>(_cubeDatas.size()));
		_sphereRender.setupRenderCommand(settings, static_cast<uint32>(_sphereDatas.size()));
		clearDebugDatas();
	}

	void debugDrawLine(const Vector3& startPos, const Vector3& endPos, const Color& color = Color::red) {
		_lineDatas.emplace_back(startPos, endPos, color);
	}

	void debugDrawCube(const Vector3& position, const Quaternion& rotation, const Vector3& extent, const Color& color = Color::red) {
		_cubeDatas.emplace_back(Matrix4::createWorldMatrix(position, rotation, extent).transpose(), color);
	}

	void debugDrawSphere(const Vector3& position, const Quaternion& rotation, const float radius, const Color& color = Color::red) {
		_sphereDatas.emplace_back(Matrix4::createWorldMatrix(position, rotation, Vector3::one * radius).transpose(), color);
	}

	void clearDebugDatas() {
		_lineDatas.clear();
		_cubeDatas.clear();
		_sphereDatas.clear();
	}

	void destroy() {
		clearDebugDatas();
		_lineRender.destroy();
		_cubeRender.destroy();
		_sphereRender.destroy();
	}

private:
	DebugLineRender _lineRender;
	DebugCubeRender _cubeRender;
	DebugSphereRender _sphereRender;

	VectorArray<DebugLineVertex> _lineDatas;
	VectorArray<DebugGeometryVertex> _cubeDatas;
	VectorArray<DebugGeometryVertex> _sphereDatas;
};