#pragma once

#include "Utility.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct BufferView;
class ConstantBuffer;
class RootSignature;
class PipelineState;
class VertexShader;
class PixelShader;

struct ConstantBufferPerFrame {
	ConstantBufferPerFrame() :constantBufferViews() {}
	~ConstantBufferPerFrame() {
		shutdown();
	}

	void shutdown();

	void create(ID3D12Device* device, const VectorArray<uint32>& bufferSizes);

	//�̒萔�o�b�t�@�͗L�����H(����������Ă���Δz�񂪂O�ȏ�Ȃ̂ł���Ŕ��f)
	bool isEnableBuffer() const {
		return constantBuffers[0].size() > 0;
	}

	//���ݐݒ肳��Ă���萔�o�b�t�@�̃f�[�^���w��t���[���̒萔�o�b�t�@�ɍX�V
	void flashBufferData(uint32 frameIndex);

	VectorArray<UniquePtr<byte[]>> dataPtrs;
	VectorArray<UniquePtr<ConstantBuffer>> constantBuffers[3];
	RefPtr<BufferView> constantBufferViews[3];
};

struct RenderSettings {
	ID3D12GraphicsCommandList* commandList;
	const uint32 frameIndex;
};

class SharedMaterial {
public:
	~SharedMaterial();

	void create(ID3D12Device* device, RefPtr<VertexShader> vertexShader, RefPtr<PixelShader> pixelShader);

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
		for (size_t i = 0; i < vertexShader->shaderReflectionResult.constantBuffers.size(); ++i) {
			const auto variable = vertexShader->shaderReflectionResult.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(vertexConstantBuffer.dataPtrs[i].get()) + variable->startOffset);
			}
		}

		//�s�N�Z���V�F�[�_�[��`����p�����[�^������
		for (size_t i = 0; i < pixelShader->shaderReflectionResult.constantBuffers.size(); ++i) {
			const auto variable = pixelShader->shaderReflectionResult.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(pixelConstantBuffer.dataPtrs[i].get()) + variable->startOffset);
			}
		}

		return nullptr;
	}

	UniquePtr<PipelineState> pipelineState;
	UniquePtr<RootSignature> rootSignature;

	RefPtr<VertexShader> vertexShader;
	RefPtr<PixelShader> pixelShader;

	RefPtr<BufferView> srvPixel;
	RefPtr<BufferView> srvVertex;

	ConstantBufferPerFrame vertexConstantBuffer;
	ConstantBufferPerFrame pixelConstantBuffer;
};