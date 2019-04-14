#pragma once

#include "Utility.h"
#include "GraphicsConstantSettings.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct BufferView;
class ConstantBuffer;
class RootSignature;
class PipelineState;
class VertexShader;
class PixelShader;

#include "PipelineState.h"

struct ConstantBufferMaterial {
	ConstantBufferMaterial();
	~ConstantBufferMaterial();

	void shutdown();

	void create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes);

	//���̒萔�o�b�t�@�͗L�����H(����������Ă���Δz�񂪂O�ȏ�Ȃ̂ł���Ŕ��f)
	bool isEnableBuffer() const;

	//���ݐݒ肳��Ă���萔�o�b�t�@�̃f�[�^���w��t���[���̒萔�o�b�t�@�ɍX�V
	void flashBufferData(uint32 frameIndex);

	VectorArray<byte*> dataPtrs;
	VectorArray<ConstantBuffer*> constantBuffers[FrameCount];
	RefPtr<BufferView> constantBufferViews[FrameCount];
};

struct Root32bitConstantMaterial {
	~Root32bitConstantMaterial();

	void create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes);
	bool isEnableConstant() const;

	VectorArray<byte*> dataPtrs;
};

struct RenderSettings {
	RenderSettings(RefPtr<ID3D12GraphicsCommandList> commandList, uint32 frameIndex) :
		commandList(commandList), frameIndex(frameIndex) {}

	RefPtr<ID3D12GraphicsCommandList> commandList;
	VectorArray<void*> vertexRoot32bitConstants;
	VectorArray<void*> pixelRoot32bitConstants;
	const uint32 frameIndex;
};

class SharedMaterial {
public:
	SharedMaterial() :srvVertex(nullptr), srvPixel(nullptr) {}
	~SharedMaterial();

	void create(RefPtr<ID3D12Device> device, RefPtr<VertexShader> vertexShader, RefPtr<PixelShader> pixelShader);

	//���̃}�e���A����`�悷�邽�߂̕`��R�}���h��ςݍ���
	void setupRenderCommand(RenderSettings& settings);

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
		const ShaderReflectionResult& vsReflection = vertexShader->shaderReflectionResult;

		//�萔�o�b�t�@
		for (size_t i = 0; i < vsReflection.constantBuffers.size(); ++i) {
			const auto variable = vsReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(vertexConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		//���[�g32bit�萔
		for (size_t i = 0; i < vsReflection.root32bitConstants.size(); ++i) {
			const auto variable = vsReflection.root32bitConstants[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(vertexRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
			}
		}

		//�s�N�Z���V�F�[�_�[��`����p�����[�^������
		const ShaderReflectionResult& psReflection = pixelShader->shaderReflectionResult;

		//�萔�o�b�t�@
		for (size_t i = 0; i < psReflection.constantBuffers.size(); ++i) {
			const auto variable = psReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(pixelConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		//���[�g32bit�萔
		for (size_t i = 0; i < psReflection.root32bitConstants.size(); ++i) {
			const auto variable = psReflection.root32bitConstants[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(pixelRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
			}
		}

		return nullptr;
	}

	PipelineState* pipelineState;
	RootSignature* rootSignature;

	RefPtr<VertexShader> vertexShader;
	RefPtr<PixelShader> pixelShader;

	RefPtr<BufferView> srvPixel;
	RefPtr<BufferView> srvVertex;

	Root32bitConstantMaterial vertexRoot32bitConstant;
	Root32bitConstantMaterial pixelRoot32bitConstant;
	ConstantBufferMaterial vertexConstantBuffer;
	ConstantBufferMaterial pixelConstantBuffer;
};