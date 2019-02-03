#include "FrameResource.h"
#include "D3D12Helper.h"
#include "D3D12Util.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"

FrameResource::FrameResource(ID3D12Device* device, IDXGISwapChain3* swapChain, UINT frameResourceIndex){
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

	//定数バッファ生成
	{
		_sceneConstant = new ConstantBuffer();
		_sceneConstant->create<SceneConstantBuffer>(device);

		//定数バッファのリソースビューを生成
		descriptorHeapManager.createConstantBufferView(_sceneConstant->getAdressOf(), &_sceneCbv, 1);
	}

	//レンダーターゲット
	{
		_renderTarget = new Texture2D();
		_renderTarget->createFromBackBuffer(swapChain, frameResourceIndex);
		descriptorHeapManager.createRenderTargetView(_renderTarget->getAdressOf(), &_rtv, 1);
	}
}


FrameResource::~FrameResource() {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	descriptorHeapManager.discardRenderTargetView(_rtv);
	descriptorHeapManager.discardConstantBufferView(_sceneCbv);

	delete _sceneConstant;
	delete _renderTarget;
}

void FrameResource::writeConstantBuffer(const SceneConstantBuffer & buffer) {
	_sceneConstant->writeData<SceneConstantBuffer>(buffer);
}