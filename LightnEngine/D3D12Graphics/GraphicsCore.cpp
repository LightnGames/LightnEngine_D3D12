#include "GraphicsCore.h"
#include <stdexcept>
#include <LMath.h>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "FrameResource.h"
#include "GpuResource.h"
#include "CommandQueue.h"
#include "CommandContext.h"
#include "DescriptorHeap.h"
#include "GpuResourceManager.h"
#include "SharedMaterial.h"
#include "MeshRenderSet.h"
#include "ImguiWindow.h"
#include "RenderableEntity.h"

#include "ThirdParty/Imgui/imgui.h"

GraphicsCore::GraphicsCore() :
	_width(1280),
	_height(720),
	_frameIndex(0),
	_viewPort({}),
	_scissorRect({}),
	_dsv(nullptr), 
	_currentFrameResource(nullptr){
}

GraphicsCore::~GraphicsCore() {
}

void GraphicsCore::onInit(HWND hwnd) {
	UINT dxgiFactoryFlags = 0;
#ifdef DEBUG
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif // DEBUG

	ComPtr<IDXGIFactory4> factory;
	throwIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	ComPtr<IDXGIAdapter1> adapter;
	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		//���zDirect3D12�Ή�GPU�͖�������
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		//�f�o�C�X�����̊m�F������������I��
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
			break;
		}
	}

	//�f�o�C�X����
	throwIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device)));

	//�f�X�N���v�^�q�[�v�}�l�[�W���[
	_descriptorHeapManager.create(_device.Get());

	//�R�}���h�L���[����
	_commandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

	//Imgui������
	_imguiWindow.init(hwnd, _device.Get());

	//�X���b�v�`�F�[������
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = _width;
	swapChainDesc.Height = _height;
	swapChainDesc.Format = RenderTargetFormat;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	throwIfFailed(factory->CreateSwapChainForHwnd(_commandContext.getDirectQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));

	//�t���X�N���[���T�|�[�g���I�t
	throwIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
	throwIfFailed(swapChain.As(&_swapChain));

	//�r���[�|�[�g������
	_viewPort = { 0.0f, 0.0f, static_cast<float>(_width), static_cast<float>(_height), 0.0f, 1.0f };
	_scissorRect = { 0, 0, static_cast<LONG>(_width), static_cast<LONG>(_height) };

	//�f�v�X�o�b�t�@����
	{
		DepthTextureInfo depthInfo;
		depthInfo.width = _width;
		depthInfo.height = _height;
		depthInfo.format = DepthStencilFormat;
		_depthStencil = makeUnique<Texture2D>();
		_depthStencil->createDepth(_device.Get(), depthInfo);

		_descriptorHeapManager.createDepthStencilView(_depthStencil->getAdressOf(), &_dsv, 1);
	}

	//�t���[�����\�[�X
	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].create(_device.Get(), _swapChain.Get(), i);
	}

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];
}

void GraphicsCore::onUpdate() {
	_imguiWindow.startFrame();
}

void GraphicsCore::onRender() {
	auto commandListSet = _commandContext.requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	//�f�X�N���v�^�q�[�v���Z�b�g
	ID3D12DescriptorHeap* ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	commandList->SetDescriptorHeaps(1, ppHeap);

	//�r���[�|�[�g�ݒ�
	commandList->RSSetViewports(1, &_viewPort);
	commandList->RSSetScissorRects(1, &_scissorRect);

	//�R�}���h�ςޗp�̃��\�[�X�o���A��W�J
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//�����_�[�^�[�Q�b�g�E�f�v�X�X�e���V���o�b�t�@���Z�b�g
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _currentFrameResource->_rtv->cpuHandle;
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &_dsv->cpuHandle);

	//�����_�[�^�[�Q�b�g�N���A
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(_dsv->cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//���b�V����`��
	RenderSettings renderSettings(commandList, _frameIndex);
	auto& meshes = _gpuResourceManager.getMeshes();
	for (const auto& mesh : meshes) {
		mesh.setupRenderCommand(renderSettings);
	}

	//ImguiWindow�`��
	_imguiWindow.renderFrame(commandList);

	//�`��p���\�[�X�o���A��W�J
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//�R�}���h�L���[�ɃR�}���h���X�g��n���Ď��s
	_commandContext.executeCommandList(commandListSet);
	_commandContext.discardCommandListSet(commandListSet);

	//��ʕ\��
	throwIfFailed(_swapChain->Present(1, 0));

	//�`�抮���܂őҋ@
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//�`�撆�ɔj�����Ă��܂��ƍ���̂ő҂�
	_commandContext.waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].shutdown();
	}

	_imguiWindow.shutdown();
	_gpuResourceManager.shutdown();

	_commandContext.shutdown();
	_descriptorHeapManager.shutdown();
	_depthStencil.reset();

	_swapChain = nullptr;
	_device = nullptr;
}

void GraphicsCore::createTextures(const VectorArray<String>& textureNames){
	_gpuResourceManager.createTextures(_device.Get(), _commandContext, textureNames);
}

void GraphicsCore::createMeshSets(const VectorArray<String>& fileNames){
	_gpuResourceManager.createMeshSets(_device.Get(), _commandContext, fileNames);
}

void GraphicsCore::createSharedMaterial(const SharedMaterialCreateSettings& settings){
	_gpuResourceManager.createSharedMaterial(_device.Get(), settings);
}

RefPtr<GpuResourceManager> GraphicsCore::getGpuResourceManager(){
	return &_gpuResourceManager;
}

void GraphicsCore::moveToNextFrame() {
	RefPtr<CommandQueue> commandQueue = _commandContext.getCommandQueue();
	const UINT64 currentFenceValue = _frameResources[_frameIndex]._fenceValue;
	commandQueue->waitForFence(currentFenceValue);

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];

	const UINT64 fenceValue = commandQueue->incrementFence();
	_frameResources[_frameIndex]._fenceValue = fenceValue;
}
