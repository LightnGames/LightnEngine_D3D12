#include "FrameResource.h"
#include "D3D12Helper.h"
#include "D3D12Util.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"

FrameResource::FrameResource(ID3D12Device* device, IDXGISwapChain3* swapChain, UINT frameResourceIndex){
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();

	//�萔�o�b�t�@����
	{
		_sceneConstant = new ConstantBuffer();
		_sceneConstant->create<SceneConstantBuffer>(device);

		//�萔�o�b�t�@�̃��\�[�X�r���[�𐶐�
		descriptorHeapManager.createConstantBufferView(_sceneConstant->getAdressOf(), &_sceneCbv, 1);
	}

	//�����_�[�^�[�Q�b�g
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