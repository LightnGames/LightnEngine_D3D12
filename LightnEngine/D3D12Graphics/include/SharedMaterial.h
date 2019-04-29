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

using Root32bitConstantInfo = std::pair<const void*, uint32>;//データポインタ、サイズ

//以下グラフィックスインターフェース定義。グラフィックスAPIインターフェースヘッダーに移動する
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

	//このコンスタントバッファーたちが有効か？
	inline constexpr bool isEnableBuffers() const {
		return views[0].isEnable();//有効であればフレームカウントずつ生成するので０番だけ見ればよい
	}
};

struct ConstantBufferMaterial {
	ConstantBufferMaterial();
	~ConstantBufferMaterial();

	void shutdown();

	void create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes);

	//この定数バッファは有効か？(初期化されていれば配列が０以上なのでそれで判断)
	bool isEnableBuffer() const;

	//現在設定されている定数バッファのデータを指定フレームの定数バッファに更新
	void flashBufferData(uint32 frameIndex);

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

//描画コマンド構築のみに必要な最小限のデータをパックした構造体
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

		//ルート32bit定数
		//for (size_t i = 0; i < _vsReflection.root32bitConstants.size(); ++i) {
		//	const auto variable = _vsReflection.root32bitConstants[i].getVariable(name);
		//	if (variable != nullptr) {
		//		return reinterpret_cast<T*>(reinterpret_cast<byte*>(_vertexRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
		//	}
		//}

		//ピクセルシェーダー定義からパラメータを検索

		//定数バッファ
		for (size_t i = 0; i < _psReflection.constantBuffers.size(); ++i) {
			const auto variable = _psReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_pixelConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		//ルート32bit定数
		//for (size_t i = 0; i < _psReflection.root32bitConstants.size(); ++i) {
		//	const auto variable = _psReflection.root32bitConstants[i].getVariable(name);
		//	if (variable != nullptr) {
		//		return reinterpret_cast<T*>(reinterpret_cast<byte*>(_pixelRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
		//	}
		//}

		return nullptr;
	}

	//コマンド構築データのみのマテリアルデータ構造体を取得
	RefSharedMaterial getRefSharedMaterial() const {
		//それぞれでデスクリプターテーブルのバインドインデックスを取得
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