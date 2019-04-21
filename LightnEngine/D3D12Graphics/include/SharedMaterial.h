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

struct ConstantBufferMaterial {
	ConstantBufferMaterial();
	~ConstantBufferMaterial();

	void shutdown();

	void create(RefPtr<ID3D12Device> device, const VectorArray<uint32>& bufferSizes);

	//この定数バッファは有効か？(初期化されていれば配列が０以上なのでそれで判断)
	bool isEnableBuffer() const;

	//現在設定されている定数バッファのデータを指定フレームの定数バッファに更新
	void flashBufferData(uint32 frameIndex);

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
	VectorArray<void*> vertexRoot32bitConstants;
	VectorArray<void*> pixelRoot32bitConstants;
	const uint32 frameIndex;
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
	void setupRenderCommand(RenderSettings& settings);

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
		for (size_t i = 0; i < _vsReflection.root32bitConstants.size(); ++i) {
			const auto variable = _vsReflection.root32bitConstants[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_vertexRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
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

		//ルート32bit定数
		for (size_t i = 0; i < _psReflection.root32bitConstants.size(); ++i) {
			const auto variable = _psReflection.root32bitConstants[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(_pixelRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
			}
		}

		return nullptr;
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