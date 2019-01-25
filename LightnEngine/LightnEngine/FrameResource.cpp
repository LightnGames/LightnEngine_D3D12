#include "FrameResource.h"
#include "D3D12Helper.h"
#include "D3D12Util.h"

FrameResource::FrameResource(ID3D12Device* device, IDXGISwapChain3* swapChain, ID3D12PipelineState* pPso, ID3D12DescriptorHeap* pRtvHeap, ID3D12DescriptorHeap* pCbvSrvHeap, UINT frameResourceIndex):
_pipelineState(pPso){

	//コマンドリスト生成
	{
		//throwIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator)));
		//throwIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), _pipelineState.Get(), IID_PPV_ARGS(&_commandList)));
		//NAME_D3D12_OBJECT(_commandList);

		//throwIfFailed(_commandList->Close()); 
	}

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
		D3D12_CONSTANT_BUFFER_VIEW_DESC constantDesc = {};
		constantDesc.BufferLocation = _constantBuffer->GetGPUVirtualAddress();
		constantDesc.SizeInBytes = 256;
		D3D12_CPU_DESCRIPTOR_HANDLE descriptorCpuHandle = pCbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE descriptorGpuHandle = pCbvSrvHeap->GetGPUDescriptorHandleForHeapStart();
		const UINT cbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)*(frameResourceIndex + 1);//0番目にテクスチャをいれてあるので...;
		
		descriptorCpuHandle.ptr += cbvSrvDescriptorSize;
		descriptorGpuHandle.ptr += cbvSrvDescriptorSize;
		device->CreateConstantBufferView(&constantDesc, descriptorCpuHandle);

		_sceneCbvHandle = descriptorGpuHandle;
	}

	//レンダーターゲット
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pRtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)*frameResourceIndex;

		throwIfFailed(swapChain->GetBuffer(frameResourceIndex, IID_PPV_ARGS(&_renderTarget)));

		D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc = {};
		rtvViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvViewDesc.Texture2D.MipSlice = 0;
		rtvViewDesc.Texture2D.PlaneSlice = 0;

		device->CreateRenderTargetView(_renderTarget.Get(), &rtvViewDesc, rtvHandle);
		NAME_D3D12_OBJECT(_renderTarget);
	}
}


FrameResource::~FrameResource() {
	//_commandAllocator = nullptr;
	//_commandList = nullptr;
	_constantBuffer = nullptr;
	_renderTarget = nullptr;
}

//void FrameResource::init() {
//	//コマンドリスト・アロケーターをリセット
//	throwIfFailed(_commandAllocator->Reset());
//	throwIfFailed(_commandList->Reset(_commandAllocator.Get(), _pipelineState.Get()));
//}

void FrameResource::writeConstantBuffer(const SceneConstantBuffer & buffer) {
	memcpy(_mappedConstantBufferData, &buffer, sizeof(SceneConstantBuffer));
}

//void FrameResource::bind() {
//	_commandList->SetGraphicsRootDescriptorTable(1, _sceneCbvHandle);
//}

//void FrameResource::presentToRenderTarget() {
//	_commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_renderTarget.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
//}

//void FrameResource::renderTargetToPresent() {
//	_commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_renderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
//}
