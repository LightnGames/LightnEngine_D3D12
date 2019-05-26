#pragma once

#include <Utility.h>
#include <d3d12.h>
#include <LMath.h>
#include "GraphicsConstantSettings.h"
#include "GpuResource.h"
#include "BufferView.h"
#include "AABB.h"

#define MAX_INSTANCE_PER_MATERIAL 256

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
class ConstantBuffer;
class VertexShader;
class PixelShader;

#include "PipelineState.h"

//�ȉ��O���t�B�b�N�X�C���^�[�t�F�[�X��`�B�O���t�B�b�N�XAPI�C���^�[�t�F�[�X�w�b�_�[�Ɉړ�����
struct SharedMaterialCreateSettings {
	String name;
	String vertexShaderName;
	String pixelShaderName;
	VectorArray<String> vsTextures;
	VectorArray<String> psTextures;
	VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts;
	D3D_PRIMITIVE_TOPOLOGY topology;
};

struct RefConstantBufferViews {
	RefBufferView views[FrameCount];

	//���̃R���X�^���g�o�b�t�@�[�������L�����H
	inline constexpr bool isEnableBuffers() const {
		return views[0].isEnable();//�L���ł���΃t���[���J�E���g����������̂łO�Ԃ�������΂悢
	}
};

struct ConstantBufferFrame {
	ConstantBufferFrame();
	~ConstantBufferFrame();

	void shutdown();

	void create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes);//�v�f���ƂɃT�C�Y���w��

	//���̒萔�o�b�t�@�͗L�����H(����������Ă���Δz�񂪂O�ȏ�Ȃ̂ł���Ŕ��f)
	bool isEnableBuffer() const;

	//���ݐݒ肳��Ă���萔�o�b�t�@�̃f�[�^���w��t���[���̒萔�o�b�t�@�ɍX�V
	void flashBufferData(uint32 frameIndex);

	//�o�b�t�@�̃f�[�^�|�C���^���X�V
	void writeBufferData(const void* dataPtr, uint32 length, uint32 bufferIndex);

	//�o�b�t�@�r���[�̎Q�Ɨp�R�s�[���t���[���o�b�t�@�����O���鐔���擾
	RefConstantBufferViews getRefBufferViews(uint32 descriptorIndex) const{
		return RefConstantBufferViews{
			constantBufferViews[0].getRefBufferView(descriptorIndex),
			constantBufferViews[1].getRefBufferView(descriptorIndex),
			constantBufferViews[2].getRefBufferView(descriptorIndex)
		};
	}

	VectorArray<byte*> dataPtrs;
	VectorArray<ConstantBuffer*> constantBuffers[FrameCount];
	BufferView constantBufferViews[FrameCount];
};

//�}�e���A���̕`��͈͒�`
struct MaterialDrawRange {
	MaterialDrawRange() :indexCount(0), indexOffset(0) {}
	MaterialDrawRange(uint32 indexCount, uint32 indexOffset) :indexCount(indexCount), indexOffset(indexOffset) {}

	uint32 indexCount;
	uint32 indexOffset;
};

struct RefVertexAndIndexBuffer {
	RefVertexAndIndexBuffer(const RefVertexBufferView& vertexView,
		const RefIndexBufferView& indexView,
		const MaterialDrawRange& drawRange) :
		vertexView(vertexView), indexView(indexView), drawRange(drawRange) {
	}

	RefVertexBufferView vertexView;
	RefIndexBufferView indexView;
	MaterialDrawRange drawRange;
};

//���_�o�b�t�@�ƃC���f�b�N�X�o�b�t�@�̃��\�[�X�Ǘ�
struct VertexAndIndexBuffer {
	VertexAndIndexBuffer(const VectorArray<MaterialDrawRange>& materialDrawRanges) :materialDrawRanges(materialDrawRanges) {}

	RefVertexAndIndexBuffer getRefVertexAndIndexBuffer(uint32 materialIndex) const {
		return RefVertexAndIndexBuffer(vertexBuffer.getRefVertexBufferView(), indexBuffer.getRefIndexBufferView(), materialDrawRanges[materialIndex]);
	}

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	AABB boundingBox;
	VectorArray<MaterialDrawRange> materialDrawRanges;
};

struct RenderSettings {
	RenderSettings(RefPtr<ID3D12GraphicsCommandList> commandList, uint32 frameIndex) :
		commandList(commandList), frameIndex(frameIndex) {}

	RefPtr<ID3D12GraphicsCommandList> commandList;
	const uint32 frameIndex;
};

struct InstanceInfoPerMaterial {
	InstanceInfoPerMaterial(RefPtr<Matrix4> mtxWorld, RefVertexAndIndexBuffer drawInfo) :
		mtxWorld(mtxWorld), drawInfo(drawInfo) {}

	RefPtr<Matrix4> mtxWorld;
	RefVertexAndIndexBuffer drawInfo;
};

class SharedMaterial {
public:
	SharedMaterial(
		const ShaderReflectionResult& vsReflection,
		const ShaderReflectionResult& psReflection,
		const RefPipelineState& pipelineState,
		const RefRootsignature& rootSignature,
		D3D_PRIMITIVE_TOPOLOGY topology);

	~SharedMaterial();

	//���̃}�e���A����`�悷�邽�߂̕`��R�}���h��ςݍ���
	void setupRenderCommand(RenderSettings& settings) const;

	//�}�e���A���ɑ����Ă��钸�_����`��
	void drawGeometries(RenderSettings& settings) const;

	//�p�����[�^���ƒl���璼�ڍX�V
	template <class T>
	void setParameter(const String& name, const T& parameter) {
		RefPtr<T> data = getParameter<T>(name);

		if (data != nullptr) {
			*data = parameter;
		}
	}

	//���O����X�V����̈�̃|�C���^���擾
	template <class T>
	RefPtr<T> getParameter(const String& name) {
		//���_�V�F�[�_�[��`����p�����[�^������

		//�萔�o�b�t�@
		for (size_t i = 0; i < _vsReflection.constantBuffers.size(); ++i) {
			const auto variable = _vsReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_vertexConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		//�s�N�Z���V�F�[�_�[��`����p�����[�^������

		//�萔�o�b�t�@
		for (size_t i = 0; i < _psReflection.constantBuffers.size(); ++i) {
			const auto variable = _psReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_pixelConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		return nullptr;
	}

	void addMeshInstance(const InstanceInfoPerMaterial& instanceInfo);
	void flushInstanceData(uint32 frameIndex);
	void setSizeInstance(RefPtr<ID3D12Device> device);

	const D3D_PRIMITIVE_TOPOLOGY _topology;

	const RefPipelineState _pipelineState;
	const RefRootsignature _rootSignature;

	const ShaderReflectionResult _vsReflection;
	const ShaderReflectionResult _psReflection;

	BufferView _srvPixel;
	BufferView _srvVertex;

	ConstantBufferFrame _vertexConstantBuffer;
	ConstantBufferFrame _pixelConstantBuffer;

	VertexBufferDynamic _instanceVertexBuffer[FrameCount];
	VectorArray<InstanceInfoPerMaterial> _meshes;
};