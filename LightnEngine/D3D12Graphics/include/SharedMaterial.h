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

	//この定数バッファは有効か？(初期化されていれば配列が０以上なのでそれで判断)
	bool isEnableBuffer() const;

	//現在設定されている定数バッファのデータを指定フレームの定数バッファに更新
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
		const ShaderReflectionResult& vsReflection = vertexShader->shaderReflectionResult;

		//定数バッファ
		for (size_t i = 0; i < vsReflection.constantBuffers.size(); ++i) {
			const auto variable = vsReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(vertexConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		//ルート32bit定数
		for (size_t i = 0; i < vsReflection.root32bitConstants.size(); ++i) {
			const auto variable = vsReflection.root32bitConstants[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(vertexRoot32bitConstant.dataPtrs[i]) + variable->startOffset);
			}
		}

		//ピクセルシェーダー定義からパラメータを検索
		const ShaderReflectionResult& psReflection = pixelShader->shaderReflectionResult;

		//定数バッファ
		for (size_t i = 0; i < psReflection.constantBuffers.size(); ++i) {
			const auto variable = psReflection.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(pixelConstantBuffer.dataPtrs[i]) + variable->startOffset);
			}
		}

		//ルート32bit定数
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