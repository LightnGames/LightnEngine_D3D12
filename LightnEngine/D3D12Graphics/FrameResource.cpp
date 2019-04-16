#include "FrameResource.h"
#include "D3D12Helper.h"
#include "D3D12Util.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"

FrameResource::FrameResource(){
}


FrameResource::~FrameResource() {
}

void FrameResource::create(RefPtr<ID3D12Device> device, RefPtr<IDXGISwapChain3> swapChain, UINT frameResourceIndex){
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

	//レンダーターゲット
	_renderTarget = new Texture2D();
	_renderTarget->createFromBackBuffer(swapChain, frameResourceIndex);
	descriptorHeapManager.createRenderTargetView(_renderTarget->getAdressOf(), &_rtv, 1);
}

void FrameResource::shutdown(){
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	descriptorHeapManager.discardRenderTargetView(_rtv);

	delete _renderTarget;
}
