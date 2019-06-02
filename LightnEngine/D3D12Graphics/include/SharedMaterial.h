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

//以下グラフィックスインターフェース定義。グラフィックスAPIインターフェースヘッダーに移動する
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

	//このコンスタントバッファーたちが有効か？
	inline constexpr bool isEnableBuffers() const {
		return views[0].isEnable();//有効であればフレームカウントずつ生成するので０番だけ見ればよい
	}
};

struct ConstantBufferFrame {
	ConstantBufferFrame();
	~ConstantBufferFrame();

	void shutdown();

	void create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes);//要素ごとにサイズを指定

	//この定数バッファは有効か？(初期化されていれば配列が０以上なのでそれで判断)
	bool isEnableBuffer() const;

	//現在設定されている定数バッファのデータを指定フレームの定数バッファに更新
	void flashBufferData(uint32 frameIndex);

	//バッファのデータポインタを更新
	void writeBufferData(const void* dataPtr, uint32 length, uint32 bufferIndex);

	//バッファビューの参照用コピーをフレームバッファリングする数分取得
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

//マテリアルの描画範囲定義
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

//頂点バッファとインデックスバッファのリソース管理
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

struct StaticMeshInstanceCommands {
	void setupRenderCommand(RenderSettings& settings) const {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		commandList->IASetVertexBuffers(0, 1, &drawInfo.vertexView.view);
		commandList->IASetIndexBuffer(&drawInfo.indexView.view);
		commandList->SetGraphicsRoot32BitConstants(2, 16, &mtxWorld, 0);
		commandList->DrawIndexedInstanced(drawInfo.drawRange.indexCount, 1, drawInfo.drawRange.indexOffset, 0, 0);
	}

	Matrix4 mtxWorld;
	RefVertexAndIndexBuffer drawInfo;
};

#include "GpuResourceManager.h"
#include "DescriptorHeap.h"
class SingleMeshRenderMaterial {
public:
	SingleMeshRenderMaterial() {
	}

	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const InitSettingsPerSingleMesh& initInfo) {
		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		
		VertexShader vs;
		PixelShader ps;
		vs.create(initInfo.vertexShaderName, inputLayouts);
		ps.create(initInfo.pixelShaderName);

		uint32 textureCount = initInfo.textureNames.size();

		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.BaseShaderRegister = 0;
		srvRange.NumDescriptors = textureCount;
		srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		VectorArray<D3D12_ROOT_PARAMETER1> parameterDescs(3);
		parameterDescs[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDescs[0].Descriptor.ShaderRegister = 0;
		parameterDescs[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDescs[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[1].DescriptorTable.NumDescriptorRanges = 1;
		parameterDescs[1].DescriptorTable.pDescriptorRanges = &srvRange;

		parameterDescs[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		parameterDescs[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDescs[2].Constants.Num32BitValues = 16;
		parameterDescs[2].Constants.ShaderRegister = 1;

		D3D12_STATIC_SAMPLER_DESC samplerDesc = WrapSamplerDesc();
		_rootSignature.create(device, parameterDescs, &samplerDesc);
		_pipelineState.create(device, &_rootSignature, &vs, &ps);

		DescriptorHeapManager& descriptorManager = DescriptorHeapManager::instance();
		GpuResourceManager& resourceManager = GpuResourceManager::instance();

		VectorArray<RefPtr<ID3D12Resource>> ppTextures(textureCount);
		for (uint32 i = 0; i < textureCount; ++i) {
			RefPtr<Texture2D> texture;
			resourceManager.loadTexture(initInfo.textureNames[i], &texture);
			ppTextures[i] = texture->get();
		}

		descriptorManager.createTextureShaderResourceView(ppTextures.data(), &_textureSrv, textureCount);
	}

	void setupRenderCommand(RenderSettings& settings, const Matrix4& mtxWorld, const RefVertexAndIndexBuffer& drawInfo) const{
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		//マテリアル設定
		commandList->SetPipelineState(_pipelineState._pipelineState.Get());
		commandList->SetGraphicsRootSignature(_rootSignature._rootSignature.Get());
		commandList->SetGraphicsRootConstantBufferView(0, settings.cameraConstantBuffer);
		commandList->SetGraphicsRootDescriptorTable(1, _textureSrv.gpuHandle);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//メッシュ描画
		commandList->IASetVertexBuffers(0, 1, &drawInfo.vertexView.view);
		commandList->IASetIndexBuffer(&drawInfo.indexView.view);
		commandList->SetGraphicsRoot32BitConstants(2, 16, &mtxWorld, 0);
		commandList->DrawIndexedInstanced(drawInfo.drawRange.indexCount, 1, drawInfo.drawRange.indexOffset, 0, 0);
	}


	void destroy() {
		_rootSignature.destroy();
		_pipelineState.destroy();
	}

	RootSignature _rootSignature;
	PipelineState _pipelineState;

	BufferView _textureSrv;
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

struct DescriptorPerFrameSet {
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
	uint32 num32bitCount;
	VectorArray<byte> dataPtr;
};

struct MaterialCommandBase {
	virtual ~MaterialCommandBase() {}

	virtual void setupCommand(RenderSettings& settings) = 0;

	VectorArray<DescriptorSet> _descriptors;
	VectorArray<DescriptorPerFrameSet> _descriptorPerFrames;
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

		for (const auto& descriptor : _descriptorPerFrames) {
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
			commandList->SetGraphicsRoot32BitConstants(rootConstant.rootParameterIndex, rootConstant.dataPtr.size() / 4, rootConstant.dataPtr.data(), 0);
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

		for (const auto& descriptor : _descriptorPerFrames) {
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
			commandList->SetComputeRoot32BitConstants(rootConstant.rootParameterIndex, rootConstant.dataPtr.size() / 4, rootConstant.dataPtr.data(), 0);
		}
	}
};