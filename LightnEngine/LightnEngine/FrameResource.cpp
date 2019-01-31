#include "FrameResource.h"
#include "D3D12Helper.h"
#include "D3D12Util.h"
#include "DescriptorHeap.h"

FrameResource::FrameResource(ID3D12Device* device, IDXGISwapChain3* swapChain, UINT frameResourceIndex){
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

	//定数バッファ生成
	{
		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = 256;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;

		device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBuffer));
		NAME_D3D12_OBJECT(_constantBuffer);
		throwIfFailed(_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_mappedConstantBufferData)));

		//定数バッファのリソースビューを生成
		descriptorHeapManager.createConstantBufferView(_constantBuffer.GetAddressOf(), &_sceneCbv, 1);
	}

	//レンダーターゲット
	{
		throwIfFailed(swapChain->GetBuffer(frameResourceIndex, IID_PPV_ARGS(&_renderTarget)));
		descriptorHeapManager.createRenderTargetView(_renderTarget.GetAddressOf(), &_rtv, 1);
	}
}


FrameResource::~FrameResource() {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	descriptorHeapManager.discardRenderTargetView(_rtv);
	descriptorHeapManager.discardConstantBufferView(_sceneCbv);
	_constantBuffer = nullptr;
	_renderTarget = nullptr;
}

void FrameResource::writeConstantBuffer(const SceneConstantBuffer & buffer) {
	memcpy(_mappedConstantBufferData, &buffer, sizeof(SceneConstantBuffer));
}