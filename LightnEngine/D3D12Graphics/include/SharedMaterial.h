#pragma once

#include <Utility.h>
#include <d3d12.h>
#include <LMath.h>
#include "GraphicsConstantSettings.h"
#include "GpuResource.h"
#include "BufferView.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct RefVertexAndIndexBuffer;
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

struct RenderSettings {
	RenderSettings(RefPtr<ID3D12GraphicsCommandList> commandList, uint32 frameIndex) :
		commandList(commandList), frameIndex(frameIndex) {}

	RefPtr<ID3D12GraphicsCommandList> commandList;
	const uint32 frameIndex;
};

//描画コマンド構築のみに必要な最小限のデータをパックした構造体
struct RefSharedMaterial {
	RefSharedMaterial(
		const RefPipelineState& pipelineState,
		const RefRootsignature& rootSignature,
		const RefBufferView& srvPixel,
		const RefBufferView& srvVertex,
		const RefConstantBufferViews& vertexConstantViews,
		const RefConstantBufferViews& pixelConstantViews,
		D3D_PRIMITIVE_TOPOLOGY topology) :
		pipelineState(pipelineState),
		rootSignature(rootSignature),
		srvPixel(srvPixel),
		srvVertex(srvVertex),
		vertexConstantViews(vertexConstantViews),
		pixelConstantViews(pixelConstantViews),
		topology(topology){}

	const RefPipelineState pipelineState;
	const RefRootsignature rootSignature;
	const RefBufferView srvPixel;
	const RefConstantBufferViews vertexConstantViews;
	const RefBufferView srvVertex;
	const RefConstantBufferViews pixelConstantViews;
	const D3D_PRIMITIVE_TOPOLOGY topology;
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

	//このマテリアルを描画するための描画コマンドを積み込む
	void setupRenderCommand(RenderSettings& settings) const;

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

	//コマンド構築データのみのマテリアルデータ構造体を取得
	RefSharedMaterial getRefSharedMaterial() const {
		//それぞれでデスクリプターテーブルのバインドインデックスを取得
		uint32 descriptorTableIndex = 0;
		uint32 srvPixelIndex = 0;
		uint32 pixelConstantIndex = 0;
		uint32 srvVertexIndex = 0;
		uint32 vertexConstantIndex = 0;

		if (_srvPixel.isEnable()) {
			srvPixelIndex = descriptorTableIndex++;
		}

		if (_pixelConstantBuffer.isEnableBuffer()) {
			pixelConstantIndex = descriptorTableIndex++;
		}

		if (_srvVertex.isEnable()) {
			srvVertexIndex = descriptorTableIndex++;
		}

		if (_vertexConstantBuffer.isEnableBuffer()) {
			vertexConstantIndex = descriptorTableIndex++;
		}

		return RefSharedMaterial(
			_pipelineState,
			_rootSignature,
			_srvPixel.getRefBufferView(srvPixelIndex),
			_srvVertex.getRefBufferView(srvVertexIndex),
			_vertexConstantBuffer.getRefBufferViews(vertexConstantIndex),
			_pixelConstantBuffer.getRefBufferViews(pixelConstantIndex),
			_topology);
	}

	void addMesh(RefPtr<ID3D12Device> device, VectorArray<RefPtr<Matrix4>> matrices, VectorArray<RefPtr<RefVertexAndIndexBuffer>>& meshes);

	const D3D_PRIMITIVE_TOPOLOGY _topology;

	const RefPipelineState _pipelineState;
	const RefRootsignature _rootSignature;

	const ShaderReflectionResult _vsReflection;
	const ShaderReflectionResult _psReflection;

	BufferView _srvPixel;
	BufferView _srvVertex;

	ConstantBufferFrame _vertexConstantBuffer;
	ConstantBufferFrame _pixelConstantBuffer;

	GpuBuffer _instanceVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW _instanceVertexView;
	VectorArray<RefPtr<Matrix4>> _matrices;
	VectorArray<RefPtr<RefVertexAndIndexBuffer>> _meshes;
};