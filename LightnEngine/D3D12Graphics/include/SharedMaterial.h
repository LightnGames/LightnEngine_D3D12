#pragma once

#include <Utility.h>
#include <d3d12.h>
#include <LMath.h>
#include "GraphicsConstantSettings.h"
#include "GpuResource.h"
#include "BufferView.h"
#include "AABB.h"

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

	void create(RefPtr<ID3D12Device> device, uint32 bufferSize);//�v�f���ƂɃT�C�Y���w��

	//���̒萔�o�b�t�@�͗L�����H(����������Ă���Δz�񂪂O�ȏ�Ȃ̂ł���Ŕ��f)
	bool isEnableBuffer() const;

	//���ݐݒ肳��Ă���萔�o�b�t�@�̃f�[�^���w��t���[���̒萔�o�b�t�@�ɍX�V
	void flashBufferData(uint32 frameIndex);

	//�o�b�t�@�̃f�[�^�|�C���^���X�V
	void writeBufferData(const void* dataPtr, uint32 length);

	VectorArray<byte> dataPtrs;
	ConstantBuffer constantBuffers[FrameCount];
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

	D3D12_VERTEX_BUFFER_VIEW getVertexView() const {
		return vertexBuffer._vertexBufferView;
	}

	D3D12_INDEX_BUFFER_VIEW getIndexView() const {
		return indexBuffer._indexBufferView;
	}

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
	AABB boundingBox;
	VectorArray<MaterialDrawRange> materialDrawRanges;
};

struct RenderSettings {
	RenderSettings(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_GPU_VIRTUAL_ADDRESS cameraConstantBuffer, uint32 frameIndex) :
		commandList(commandList), cameraConstantBuffer(cameraConstantBuffer), frameIndex(frameIndex) {}

	RefPtr<ID3D12GraphicsCommandList> commandList;
	const D3D12_GPU_VIRTUAL_ADDRESS cameraConstantBuffer;
	const uint32 frameIndex;
};

struct InstanceInfoPerMaterial {
	InstanceInfoPerMaterial(Matrix4 mtxWorld, RefVertexAndIndexBuffer drawInfo) :
		mtxWorld(mtxWorld), drawInfo(drawInfo) {}

	Matrix4 mtxWorld;
	RefVertexAndIndexBuffer drawInfo;
};

struct InitSettingsPerSingleMesh {
	String getShaderNameSet() const {
		return vertexShaderName + "_" + pixelShaderName;
	}

	String vertexShaderName;
	String pixelShaderName;
	VectorArray<String> textureNames;
};

enum class ResourceType {
	CONSTANT_BUFFER,
	SHADER_RESOURCE
};

struct DescriptorSet {
	DescriptorSet() {}
	DescriptorSet(uint32 rootParameterIndex, RefBufferView viewAddress)
		:rootParameterIndex(rootParameterIndex), viewAddress(viewAddress) {}
	uint32 rootParameterIndex;
	RefBufferView viewAddress;
};

struct GpuResourcePerFrameSet {
	uint32 rootParameterIndex;
	D3D12_GPU_VIRTUAL_ADDRESS resourceAddress[FrameCount];
	ResourceType type;
};

struct GpuResourceSet {
	uint32 rootParameterIndex;
	D3D12_GPU_VIRTUAL_ADDRESS resourceAddress;
	ResourceType type;
};

struct RootConstantSet {
	uint32 rootParameterIndex;
	VectorArray<byte> dataPtr;
};

struct MaterialCommandBase {
	virtual ~MaterialCommandBase() {}

	virtual void setupCommand(RenderSettings& settings) = 0;

	VectorArray<DescriptorSet> _descriptors;
	VectorArray<GpuResourcePerFrameSet> _gpuResourcePerFrames;
	VectorArray<GpuResourceSet> _gpuResources;
	VectorArray<RootConstantSet> _rootConstants;
	D3D12_PRIMITIVE_TOPOLOGY _topology;
	RefRootSignature _rootSignature;
	RefPipelineState _pipelineState;
};

struct MaterialCommandGraphics :public MaterialCommandBase {
	void setupCommand(RenderSettings& settings) override {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		const uint32 frameIndex = settings.frameIndex;

		commandList->SetPipelineState(_pipelineState.pipelineState);
		commandList->SetGraphicsRootSignature(_rootSignature.rootSignature);
		commandList->IASetPrimitiveTopology(_topology);

		for (const auto& descriptor : _descriptors) {
			commandList->SetGraphicsRootDescriptorTable(descriptor.rootParameterIndex, descriptor.viewAddress.gpuHandle);
		}

		for (const auto& descriptor : _gpuResourcePerFrames) {
			switch (descriptor.type) {
			case ResourceType::CONSTANT_BUFFER:
				commandList->SetGraphicsRootConstantBufferView(descriptor.rootParameterIndex, descriptor.resourceAddress[frameIndex]);
				break;

			case ResourceType::SHADER_RESOURCE:
				commandList->SetGraphicsRootShaderResourceView(descriptor.rootParameterIndex, descriptor.resourceAddress[frameIndex]);
				break;
			}
		}

		for (const auto& gpuResource : _gpuResources) {
			switch (gpuResource.type) {
			case ResourceType::CONSTANT_BUFFER:
				commandList->SetGraphicsRootConstantBufferView(gpuResource.rootParameterIndex, gpuResource.resourceAddress);
				break;

			case ResourceType::SHADER_RESOURCE:
				commandList->SetGraphicsRootShaderResourceView(gpuResource.rootParameterIndex, gpuResource.resourceAddress);
				break;
			}
		}

		for (const auto& rootConstant : _rootConstants) {
			const uint32 num32bitCount = static_cast<uint32>(rootConstant.dataPtr.size() / 4);
			commandList->SetGraphicsRoot32BitConstants(rootConstant.rootParameterIndex, num32bitCount, rootConstant.dataPtr.data(), 0);
		}
	}
};

struct MaterialCommandCompute :public MaterialCommandBase {
	void setupCommand(RenderSettings& settings) override {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		const uint32 frameIndex = settings.frameIndex;

		commandList->SetPipelineState(_pipelineState.pipelineState);
		commandList->SetComputeRootSignature(_rootSignature.rootSignature);

		for (const auto& descriptor : _descriptors) {
			commandList->SetComputeRootDescriptorTable(descriptor.rootParameterIndex, descriptor.viewAddress.gpuHandle);
		}

		for (const auto& descriptor : _gpuResourcePerFrames) {
			switch (descriptor.type) {
			case ResourceType::CONSTANT_BUFFER:
				commandList->SetComputeRootConstantBufferView(descriptor.rootParameterIndex, descriptor.resourceAddress[frameIndex]);
				break;

			case ResourceType::SHADER_RESOURCE:
				commandList->SetComputeRootShaderResourceView(descriptor.rootParameterIndex, descriptor.resourceAddress[frameIndex]);
				break;
			}
		}

		for (const auto& gpuResource : _gpuResources) {
			switch (gpuResource.type) {
			case ResourceType::CONSTANT_BUFFER:
				commandList->SetComputeRootConstantBufferView(gpuResource.rootParameterIndex, gpuResource.resourceAddress);
				break;

			case ResourceType::SHADER_RESOURCE:
				commandList->SetComputeRootShaderResourceView(gpuResource.rootParameterIndex, gpuResource.resourceAddress);
				break;
			}
		}

		for (const auto& rootConstant : _rootConstants) {
			const uint32 num32bitCount = static_cast<uint32>(rootConstant.dataPtr.size() / 4);
			commandList->SetComputeRoot32BitConstants(rootConstant.rootParameterIndex, num32bitCount, rootConstant.dataPtr.data(), 0);
		}
	}
};