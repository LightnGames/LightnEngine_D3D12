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

struct DebugCapsuleVertex {
	DebugCapsuleVertex(const Matrix4& mtxWorld, float height, const Color& color) :
		mtxWorld(mtxWorld), height(height), color(color) {}

	Matrix4 mtxWorld;
	Color color;
	float height;
};

//デバッグ描画基底クラス　バックバッファごとの動的頂点バッファを持っている
class DebugRender :private NonCopyable {
public:
	virtual ~DebugRender();

	//インスタンスバッファにデータを書き込み
	void writeBufferDara(uint32 frameIndex, void* srcPtr, size_t bufferLength);
	virtual void destroy();

protected:
	RefPtr<SharedMaterial> _material;
	VertexBufferDynamic _perInstanceData[FrameCount];
};

//デバッグワイヤーライン描画クラス
class DebugLineRender :public DebugRender{
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount) const;
	void destroy() override;
};

//デバッグワイヤーキューブ描画クラス
class DebugCubeRender :public DebugRender{
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount) const;
	void destroy() override;

private:
	VertexBuffer _geometryVertexBuffer;
};

//デバッグワイヤースフィア描画クラス
class DebugSphereRender :public DebugRender {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount) const;
	void destroy() override;

private:
	VertexBuffer _geometryVertexBuffer;
};

//デバッグワイヤーカプセル描画クラス
class DebugCapsuleRender :public DebugRender {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount) const;
	void destroy() override;

private:
	VertexBuffer _geometryVertexBuffer;
};

class DebugGeometryRender :public Singleton<DebugGeometryRender> {
public:
	DebugGeometryRender();

	//デバッグジオメトリインスタンスを初期化＆生成
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);

	//インスタンスごとのデータをマップする
	void updatePerInstanceData(uint32 frameIndex);

	//デバッグジオメトリの描画コマンドを発行
	void setupRenderCommand(RenderSettings& settings) const;

	//デバッグジオメトリの描画リストを追加
	void debugDrawLine(const Vector3& startPos, const Vector3& endPos, const Color& color = Color::red);
	void debugDrawCube(const Vector3& position, const Quaternion& rotation, const Vector3& extent, const Color& color = Color::red);
	void debugDrawSphere(const Vector3& position, const Quaternion& rotation, float radius, const Color& color = Color::red);
	void debugDrawCapsule(const Vector3& position, const Quaternion& rotation, float radius, float height, const Color& color = Color::red);

	//デバッグジオメトリのデータ配列をクリーンアップ
	void clearDebugDatas();

	//破棄
	void destroy();

private:
	DebugLineRender _lineRender;
	DebugCubeRender _cubeRender;
	DebugSphereRender _sphereRender;
	DebugCapsuleRender _capsuleRender;

	VectorArray<DebugLineVertex> _lineDatas;
	VectorArray<DebugGeometryVertex> _cubeDatas;
	VectorArray<DebugGeometryVertex> _sphereDatas;
	VectorArray<DebugCapsuleVertex> _capsuleDatas;
};