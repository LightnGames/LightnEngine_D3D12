#pragma once

#include <Utility.h>
#include <d3d12.h>
#include <LMath.h>
#include "GraphicsConstantSettings.h"
#include "GpuResource.h"
#include "BufferView.h"
#include "AABB.h"
#include "RenderPass.h"

#define MAX_INSTANCE_PER_MATERIAL 256

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
	InstanceInfoPerMaterial(RefPtr<Matrix4> mtxWorld, RefVertexAndIndexBuffer drawInfo) :
		mtxWorld(mtxWorld), drawInfo(drawInfo) {}

	RefPtr<Matrix4> mtxWorld;
	RefVertexAndIndexBuffer drawInfo;
};

class SingleMeshRenderPass : public IRenderPass {
public:
	SingleMeshRenderPass(
		const ShaderReflectionResult& vsReflection,
		const ShaderReflectionResult& psReflection,
		const RefPipelineState& pipelineState,
		const RefRootsignature& rootSignature,
		D3D_PRIMITIVE_TOPOLOGY topology);

	~SingleMeshRenderPass();

	//このマテリアルを描画するための描画コマンドを積み込む
	void setupRenderCommand(RenderSettings& settings) override;

	//マテリアルに属している頂点情報を描画
	void drawGeometries(RenderSettings& settings) const;

	void destroy() override;

	//パラメータ名と値から直接更新
	template <class T>
	void setParameter(const String& name, const T& parameter) {
		RefPtr<T> data = getParameter<T>(name);

		if (data != nullptr) {
			*data = parameter;
		}
	}

	//名前から更新する領域のポインタを取得
	template <class T>
	RefPtr<T> getParameter(const String& name) {
		//頂点シェーダー定義からパラメータを検索

		//定数バッファ
		for (size_t i = 0; i < _vsReflection.constantBuffers.size(); ++i) {
			const auto variable = _vsReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_vertexConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		//ピクセルシェーダー定義からパラメータを検索

		//定数バッファ
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

struct InitSettingsPerSingleMesh {
	String vertexShaderName;
	String pixelShaderName;
	VectorArray<String> textureNames;
};

#include "GpuResourceManager.h"
#include "DescriptorHeap.h"
class SingleMeshRenderPass2 : public IRenderPass {
public:
	SingleMeshRenderPass2() {
	}

	void create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const InitSettingsPerSingleMesh& initInfo) {
		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
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

		VectorArray<D3D12_ROOT_PARAMETER1> parameterDescs(2);
		parameterDescs[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDescs[0].Descriptor.ShaderRegister = 0;
		parameterDescs[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDescs[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[1].DescriptorTable.NumDescriptorRanges = 1;
		parameterDescs[1].DescriptorTable.pDescriptorRanges = &srvRange;

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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

		_meshes.reserve(MAX_INSTANCE_PER_MATERIAL);

		for (uint32 i = 0; i < FrameCount; ++i) {
			_instanceVertexBuffer[i].createDirectEmptyVertex(device, sizeof(Matrix4), MAX_INSTANCE_PER_MATERIAL);
		}
	}

	void setupRenderCommand(RenderSettings& settings) override {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		const uint32 frameIndex = settings.frameIndex;

		VectorArray<Matrix4> m;
		m.reserve(_meshes.size());
		for (const auto& mesh : _meshes) {
			m.emplace_back(mesh.mtxWorld->transpose());
		}

		_instanceVertexBuffer[frameIndex].writeData(m.data(), static_cast<uint32>(m.size() * sizeof(Matrix4)));

		commandList->SetPipelineState(_pipelineState._pipelineState.Get());
		commandList->SetGraphicsRootSignature(_rootSignature._rootSignature.Get());

		commandList->SetGraphicsRootConstantBufferView(0, settings.cameraConstantBuffer);
		commandList->SetGraphicsRootDescriptorTable(1, _textureSrv.gpuHandle);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (size_t i = 0; i < _meshes.size(); ++i) {
			const InstanceInfoPerMaterial& mesh = _meshes[i];
			const RefVertexAndIndexBuffer& drawInfo = mesh.drawInfo;
			D3D12_VERTEX_BUFFER_VIEW views[2] = { drawInfo.vertexView.view, _instanceVertexBuffer[frameIndex]._vertexBufferView };

			commandList->IASetVertexBuffers(0, 2, views);
			commandList->IASetIndexBuffer(&drawInfo.indexView.view);
			commandList->DrawIndexedInstanced(drawInfo.drawRange.indexCount, 1, drawInfo.drawRange.indexOffset, 0, i);
		}
	}

	void destroy() override {

	}

	void addMeshInstance(const InstanceInfoPerMaterial& instanceInfo) {
		_meshes.emplace_back(instanceInfo);
	}

	RootSignature _rootSignature;
	PipelineState _pipelineState;

	BufferView _textureSrv;

	VertexBufferDynamic _instanceVertexBuffer[FrameCount];
	VectorArray<InstanceInfoPerMaterial> _meshes;
};