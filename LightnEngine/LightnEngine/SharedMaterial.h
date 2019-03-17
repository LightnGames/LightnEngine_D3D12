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

	//個の定数バッファは有効か？(初期化されていれば配列が０以上なのでそれで判断)
	bool isEnableBuffer() const {
		return constantBuffers[0].size() > 0;
	}

	//現在設定されている定数バッファのデータを指定フレームの定数バッファに更新
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
		for (size_t i = 0; i < vertexShader->shaderReflectionResult.constantBuffers.size(); ++i) {
			const auto variable = vertexShader->shaderReflectionResult.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(vertexConstantBuffer.dataPtrs[i].get()) + variable->startOffset);
			}
		}

		//ピクセルシェーダー定義からパラメータを検索
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