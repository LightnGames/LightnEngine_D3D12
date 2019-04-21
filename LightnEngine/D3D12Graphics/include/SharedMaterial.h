#pragma once

#include <Utility.h>
#include "GraphicsConstantSettings.h"
#include "BufferView.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
class ConstantBuffer;
class VertexShader;
class PixelShader;

#include "PipelineState.h"

using Root32bitConstantInfo = std::pair<const void*, uint32>;//�f�[�^�|�C���^�A�T�C�Y

struct RefConstantBufferViews {
	RefBufferView views[FrameCount];

	//���̃R���X�^���g�o�b�t�@�[�������L�����H
	inline bool isEnableBuffers() const {
		return views[0].isEnable();//�L���ł���΃t���[���J�E���g����������̂łO�Ԃ�������΂悢
	}
};

struct ConstantBufferMaterial {
	ConstantBufferMaterial();
	~ConstantBufferMaterial();

	void shutdown();

	void create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes);

	//���̒萔�o�b�t�@�͗L�����H(����������Ă���Δz�񂪂O�ȏ�Ȃ̂ł���Ŕ��f)
	bool isEnableBuffer() const;

	//���ݐݒ肳��Ă���萔�o�b�t�@�̃f�[�^���w��t���[���̒萔�o�b�t�@�ɍX�V
	void flashBufferData(uint32 frameIndex);

	//�o�b�t�@�r���[�̎Q�Ɨp�R�s�[���t���[���o�b�t�@�����O���鐔���擾
	RefConstantBufferViews getRefBufferViews() const{
		return RefConstantBufferViews{
			constantBufferViews[0].getRefBufferView(),
			constantBufferViews[1].getRefBufferView(),
			constantBufferViews[2].getRefBufferView()
		};
	}

	VectorArray<byte*> dataPtrs;
	VectorArray<ConstantBuffer*> constantBuffers[FrameCount];
	BufferView constantBufferViews[FrameCount];
};

struct Root32bitConstantMaterial {
	~Root32bitConstantMaterial();

	void create(const VectorArray<uint32>& bufferSizes);
	bool isEnableConstant() const;

	VectorArray<byte*> dataPtrs;
};

struct RenderSettings {
	RenderSettings(RefPtr<ID3D12GraphicsCommandList> commandList, uint32 frameIndex) :
		commandList(commandList), frameIndex(frameIndex) {}

	RefPtr<ID3D12GraphicsCommandList> commandList;
	VectorArray<Root32bitConstantInfo> vertexRoot32bitConstants;
	VectorArray<Root32bitConstantInfo> pixelRoot32bitConstants;
	const uint32 frameIndex;
};

//�`��R�}���h�\�z�݂̂ɕK�v�ȍŏ����̃f�[�^���p�b�N�����\����
struct RefSharedMaterial {
	RefSharedMaterial(
		const RefPipelineState& pipelineState,
		const RefRootsignature& rootSignature,
		const RefBufferView& srvPixel,
		const RefBufferView& srvVertex,
		const RefConstantBufferViews& vertexConstantViews,
		const RefConstantBufferViews& pixelConstantViews) :
		_pipelineState(pipelineState),
		_rootSignature(rootSignature),
		_srvPixel(srvPixel),
		_srvVertex(srvVertex),
		_vertexConstantViews(vertexConstantViews),
		_pixelConstantViews(pixelConstantViews) {}

	//�`��R�}���h�\�z
	void setupRenderCommand(RenderSettings& settings) const;

	const RefPipelineState _pipelineState;
	const RefRootsignature _rootSignature;
	const RefBufferView _srvPixel;
	const RefBufferView _srvVertex;
	const RefConstantBufferViews _vertexConstantViews;
	const RefConstantBufferViews _pixelConstantViews;
};

class SharedMaterial {
public:
	SharedMaterial(
		const ShaderReflectionResult& vsReflection,
		const ShaderReflectionResult& psReflection,
		const RefPipelineState& pipelineState,
		const RefRootsignature& rootSignature);

	~SharedMaterial();

	//���̃}�e���A����`�悷�邽�߂̕`��R�}���h��ςݍ���
	void setupRenderCommand(RenderSettings& settings) const;

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

		//���[�g32bit�萔
		for (size_t i = 0; i < _vsReflection.root32bitConstants.size(); ++i) {
			const auto variable = _vsReflection.root32bitConstants[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_vertexRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
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

		//���[�g32bit�萔
		for (size_t i = 0; i < _psReflection.root32bitConstants.size(); ++i) {
			const auto variable = _psReflection.root32bitConstants[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_pixelRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
			}
		}

		return nullptr;
	}

	//�R�}���h�\�z�f�[�^�݂̂̃}�e���A���f�[�^�\���̂��擾
	RefSharedMaterial getRefSharedMaterial() const {
		return RefSharedMaterial(_pipelineState,_rootSignature,
			_srvPixel.getRefBufferView(),
			_srvVertex.getRefBufferView(),
			_vertexConstantBuffer.getRefBufferViews(),
			_pixelConstantBuffer.getRefBufferViews());
	}

	const RefPipelineState _pipelineState;
	const RefRootsignature _rootSignature;

	const ShaderReflectionResult _vsReflection;
	const ShaderReflectionResult _psReflection;

	BufferView _srvPixel;
	BufferView _srvVertex;

	Root32bitConstantMaterial _vertexRoot32bitConstant;
	Root32bitConstantMaterial _pixelRoot32bitConstant;
	ConstantBufferMaterial _vertexConstantBuffer;
	ConstantBufferMaterial _pixelConstantBuffer;
};