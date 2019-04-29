#pragma once

#include <Utility.h>
#include <d3d12.h>
#include "GraphicsConstantSettings.h"
#include "BufferView.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
class ConstantBuffer;
class VertexShader;
class PixelShader;

#include "PipelineState.h"

using Root32bitConstantInfo = std::pair<const void*, uint32>;//�f�[�^�|�C���^�A�T�C�Y

//�ȉ��O���t�B�b�N�X�C���^�[�t�F�[�X��`�B�O���t�B�b�N�XAPI�C���^�[�t�F�[�X�w�b�_�[�Ɉړ�����
struct SharedMaterialCreateSettings {
	String name;
	String vertexShaderName;
	String pixelShaderName;
	VectorArray<String> vsTextures;
	VectorArray<String> psTextures;
	VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts;
};

struct RefConstantBufferViews {
	RefBufferView views[FrameCount];

	//���̃R���X�^���g�o�b�t�@�[�������L�����H
	inline constexpr bool isEnableBuffers() const {
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
		const RefConstantBufferViews& pixelConstantViews,
		const uint32 pixelRoot32bitConstantIndex,
		const uint32 pixelRoot32bitConstantCount,
		const uint32 vertexRoot32bitConstantIndex,
		const uint32 vertexRoot32bitConstantCount) :
		pipelineState(pipelineState),
		rootSignature(rootSignature),
		srvPixel(srvPixel),
		srvVertex(srvVertex),
		vertexConstantViews(vertexConstantViews),
		pixelConstantViews(pixelConstantViews),
		pixelRoot32bitConstantIndex(pixelRoot32bitConstantIndex),
		pixelRoot32bitConstantCount(pixelRoot32bitConstantCount),
		vertexRoot32bitConstantIndex(vertexRoot32bitConstantIndex),
		vertexRoot32bitConstantCount(vertexRoot32bitConstantCount){}

	const RefPipelineState pipelineState;
	const RefRootsignature rootSignature;
	const RefBufferView srvPixel;
	const RefConstantBufferViews vertexConstantViews;
	const uint32 pixelRoot32bitConstantIndex;
	const uint32 pixelRoot32bitConstantCount;
	const RefBufferView srvVertex;
	const RefConstantBufferViews pixelConstantViews;
	const uint32 vertexRoot32bitConstantIndex;
	const uint32 vertexRoot32bitConstantCount;
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
		//for (size_t i = 0; i < _vsReflection.root32bitConstants.size(); ++i) {
		//	const auto variable = _vsReflection.root32bitConstants[i].getVariable(name);
		//	if (variable != nullptr) {
		//		return reinterpret_cast<T*>(reinterpret_cast<byte*>(_vertexRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
		//	}
		//}

		//�s�N�Z���V�F�[�_�[��`����p�����[�^������

		//�萔�o�b�t�@
		for (size_t i = 0; i < _psReflection.constantBuffers.size(); ++i) {
			const auto variable = _psReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_pixelConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		//���[�g32bit�萔
		//for (size_t i = 0; i < _psReflection.root32bitConstants.size(); ++i) {
		//	const auto variable = _psReflection.root32bitConstants[i].getVariable(name);
		//	if (variable != nullptr) {
		//		return reinterpret_cast<T*>(reinterpret_cast<byte*>(_pixelRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
		//	}
		//}

		return nullptr;
	}

	//�R�}���h�\�z�f�[�^�݂̂̃}�e���A���f�[�^�\���̂��擾
	RefSharedMaterial getRefSharedMaterial() const {
		//���ꂼ��Ńf�X�N���v�^�[�e�[�u���̃o�C���h�C���f�b�N�X���擾
		uint32 descriptorTableIndex = 0;
		uint32 srvPixelIndex = 0;
		uint32 pixelConstantIndex = 0;
		uint32 pixelRoot32bitIndex = 0;
		uint32 pixelRoot32bitCount = 0;
		uint32 srvVertexIndex = 0;
		uint32 vertexConstantIndex = 0;
		uint32 vertexRoot32bitIndex = 0;
		uint32 vertexRoot32bitCount = 0;

		if (_srvPixel.isEnable()) {
			srvPixelIndex = descriptorTableIndex++;
		}

		if (_pixelConstantBuffer.isEnableBuffer()) {
			pixelConstantIndex = descriptorTableIndex++;
		}

		pixelRoot32bitCount = static_cast<uint32>(_psReflection.root32bitConstants.size());
		if (pixelRoot32bitCount > 0) {
			pixelRoot32bitIndex = descriptorTableIndex;
			descriptorTableIndex += pixelRoot32bitCount;
		}

		if (_srvVertex.isEnable()) {
			srvVertexIndex = descriptorTableIndex++;
		}

		if (_vertexConstantBuffer.isEnableBuffer()) {
			vertexConstantIndex = descriptorTableIndex++;
		}

		vertexRoot32bitCount = static_cast<uint32>(_vsReflection.root32bitConstants.size());
		if (vertexRoot32bitCount > 0) {
			vertexRoot32bitIndex = descriptorTableIndex;
			descriptorTableIndex += vertexRoot32bitCount;
		}

		return RefSharedMaterial(
			_pipelineState,
			_rootSignature,
			_srvPixel.getRefBufferView(srvPixelIndex),
			_srvVertex.getRefBufferView(srvVertexIndex),
			_vertexConstantBuffer.getRefBufferViews(vertexConstantIndex),
			_pixelConstantBuffer.getRefBufferViews(pixelConstantIndex),
			pixelRoot32bitIndex,
			pixelRoot32bitCount,
			vertexRoot32bitIndex,
			vertexRoot32bitCount);
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