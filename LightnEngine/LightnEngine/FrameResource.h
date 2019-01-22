#pragma once

#include "stdafx.h"

using namespace Microsoft::WRL;
class FrameResource {
public:
	FrameResource(ID3D12Device* device, IDXGISwapChain3* swapChain, ID3D12PipelineState* pPso, ID3D12DescriptorHeap* pRtvHeap, ID3D12DescriptorHeap* pCbvSrvHeap, UINT frameResourceIndex);
	~FrameResource();

	//void init();
	void writeConstantBuffer(const SceneConstantBuffer& buffer);
	//void bind();//デスクリプタテーブルにデータをセット
	//void presentToRenderTarget();//PresentからRenderTargetに遷移するリソースバリアを張る
	//void renderTargetToPresent();

	//ComPtr<ID3D12CommandAllocator> _commandAllocator;
	//ComPtr<ID3D12GraphicsCommandList> _commandList;
	ComPtr<ID3D12PipelineState> _pipelineState;

	UINT64 _fenceValue;

	ComPtr<ID3D12Resource> _constantBuffer;
	ComPtr<ID3D12Resource> _renderTarget;

	SceneConstantBuffer* _mappedConstantBufferData;
	D3D12_GPU_DESCRIPTOR_HANDLE _sceneCbvHandle;
private:
};

