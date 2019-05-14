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

//�f�o�b�O�`����N���X�@�o�b�N�o�b�t�@���Ƃ̓��I���_�o�b�t�@�������Ă���
class DebugRender :private NonCopyable {
public:
	virtual ~DebugRender();

	//�C���X�^���X�o�b�t�@�Ƀf�[�^����������
	void writeBufferDara(uint32 frameIndex, void* srcPtr, size_t bufferLength);
	virtual void destroy();

protected:
	RefPtr<SharedMaterial> _material;
	VertexBufferDynamic _perInstanceData[FrameCount];
};

//�f�o�b�O���C���[���C���`��N���X
class DebugLineRender :public DebugRender{
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount) const;
	void destroy() override;
};

//�f�o�b�O���C���[�L���[�u�`��N���X
class DebugCubeRender :public DebugRender{
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount) const;
	void destroy() override;

private:
	VertexBuffer _geometryVertexBuffer;
};

//�f�o�b�O���C���[�X�t�B�A�`��N���X
class DebugSphereRender :public DebugRender {
public:
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);
	void setupRenderCommand(RenderSettings& settings, uint32 instanceCount) const;
	void destroy() override;

private:
	VertexBuffer _geometryVertexBuffer;
};

//�f�o�b�O���C���[�J�v�Z���`��N���X
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

	//�f�o�b�O�W�I���g���C���X�^���X��������������
	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext);

	//�C���X�^���X���Ƃ̃f�[�^���}�b�v����
	void updatePerInstanceData(uint32 frameIndex);

	//�f�o�b�O�W�I���g���̕`��R�}���h�𔭍s
	void setupRenderCommand(RenderSettings& settings) const;

	//�f�o�b�O�W�I���g���̕`�惊�X�g��ǉ�
	void debugDrawLine(const Vector3& startPos, const Vector3& endPos, const Color& color = Color::red);
	void debugDrawCube(const Vector3& position, const Quaternion& rotation, const Vector3& extent, const Color& color = Color::red);
	void debugDrawSphere(const Vector3& position, const Quaternion& rotation, float radius, const Color& color = Color::red);
	void debugDrawCapsule(const Vector3& position, const Quaternion& rotation, float radius, float height, const Color& color = Color::red);

	//�f�o�b�O�W�I���g���̃f�[�^�z����N���[���A�b�v
	void clearDebugDatas();

	//�j��
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