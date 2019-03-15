#pragma once

#include "Utility.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"

struct ConstantBufferPerFrame {
	ConstantBufferPerFrame() :constantBufferViews() {}
	~ConstantBufferPerFrame() {
		shutdown();
	}

	void shutdown() {
		DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
		for (auto&& cbv : constantBufferViews) {
			if (cbv != nullptr) {
				descriptorHeapManager.discardConstantBufferView(cbv);
			}
		}

		for (uint32 i = 0; i < FrameCount; ++i) {

			for (auto&& cb : constantBuffers[i]) {
				cb.reset();
			}

			constantBuffers[i].clear();
		}
	}

	void create(ID3D12Device* device, const VectorArray<uint32>& bufferSizes) {
		const uint32 constantBufferCount = static_cast<uint32>(bufferSizes.size());
		DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

		for (int i = 0; i < FrameCount; ++i) {
			constantBuffers[i].resize(constantBufferCount);

			UniquePtr<ID3D12Resource*[]> p = makeUnique<ID3D12Resource*[]>(constantBufferCount);
			for (uint32 j = 0; j < constantBufferCount; ++j) {
				constantBuffers[i][j] = makeUnique<ConstantBuffer>();
				constantBuffers[i][j]->create(device, bufferSizes[j]);
				p[j] = constantBuffers[i][j]->get();
			}

			descriptorHeapManager.createConstantBufferView(p.get(), &constantBufferViews[i], constantBufferCount);
		}
	}

	VectorArray<UniquePtr<ConstantBuffer>> constantBuffers[FrameCount];
	RefPtr<BufferView> constantBufferViews[FrameCount];
};

struct RenderSettings {
	ID3D12GraphicsCommandList* commandList;
};

class SharedMaterial {
public:
	~SharedMaterial() {
		DescriptorHeapManager& manager = DescriptorHeapManager::instance();
		if (srvPixel != nullptr) {
			manager.discardShaderResourceView(srvPixel);
		}
	}

	void create(ID3D12Device* device, RefPtr<VertexShader> vertexShader, RefPtr<PixelShader> pixelShader) {
		rootSignature.create(device, *vertexShader, *pixelShader);
		pipelineState.create(device, &rootSignature, *vertexShader, *pixelShader);

		this->vertexShader = vertexShader;
		this->pixelShader = pixelShader;
	}

	void setupRenderCommand(RenderSettings& settings) {
		ID3D12GraphicsCommandList* commandList = settings.commandList;
		commandList->SetPipelineState(pipelineState._pipelineState.Get());
		commandList->SetGraphicsRootSignature(rootSignature._rootSignature.Get());

		//デスクリプタヒープにセットした定数バッファをセット
		commandList->SetGraphicsRootDescriptorTable(0, srvPixel->gpuHandle);
		commandList->SetGraphicsRootDescriptorTable(1, vertexConstantBuffer.constantBufferViews[frameIndex]->gpuHandle);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	template <class T>
	void setParameter(const String& name, const T& parameter) {
		RefPtr<T> data = getParameter<T>(name);
		*data = parameter;
	}

	template <class T>
	RefPtr<T> getParameter(const String& name) {
		auto& vertexCb = vertexConstantBuffer.constantBuffers[frameIndex];
		for (size_t i = 0; i < vertexShader->result.constantBuffers.size(); ++i) {
			const auto variable = vertexShader->result.constantBuffers[i].getVariable(name);
			if (variable != nullptr) {
				return reinterpret_cast<T*>(reinterpret_cast<byte*>(vertexCb[i]->dataPtr) + variable->startOffset);
			}
		}

		return nullptr;
	}

	PipelineState pipelineState;
	RootSignature rootSignature;

	RefPtr<VertexShader> vertexShader;
	RefPtr<PixelShader> pixelShader;

	RefPtr<BufferView> srvPixel;
	RefPtr<BufferView> srvVertex;

	ConstantBufferPerFrame vertexConstantBuffer;
	ConstantBufferPerFrame pixelConstantBuffer;

	static uint32 frameIndex;
};