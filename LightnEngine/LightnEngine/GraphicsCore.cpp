#include "GraphicsCore.h"
#include <d3dcompiler.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "FrameResource.h"
#include "GpuResource.h"
#include "CommandQueue.h"
#include "CommandContext.h"
#include "DescriptorHeap.h"
#include "PipelineState.h"
#include "GpuResourceManager.h"
#include "SharedMaterial.h"

#include "Utility.h"

GraphicsCore::GraphicsCore() {
	_width = 1280;
	_height = 720;
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
	_descriptorHeapManager = new DescriptorHeapManager();
	_descriptorHeapManager->create(_device.Get());

	//�R�}���h�L���[����
	_commandContext = new CommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	_commandContext->create(_device.Get());

	//���\�[�X�}�l�[�W���[
	_gpuResourceManager = new GpuResourceManager();

	//�X���b�v�`�F�[������
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = _width;
	swapChainDesc.Height = _height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	throwIfFailed(factory->CreateSwapChainForHwnd(_commandContext->commandQueue()->commandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));

	//�t���X�N���[���T�|�[�g���I�t
	throwIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
	throwIfFailed(swapChain.As(&_swapChain));

	//�r���[�|�[�g������
	_viewPort = { 0.0f, 0.0f, static_cast<float>(_width), static_cast<float>(_height) };
	_scissorRect = { 0, 0, static_cast<LONG>(_width), static_cast<LONG>(_height) };

	//�f�v�X�o�b�t�@����
	{
		DepthTextureInfo depthInfo;
		depthInfo.width = _width;
		depthInfo.height = _height;
		depthInfo.format = DXGI_FORMAT_D32_FLOAT;
		_depthStencil = new Texture2D();
		_depthStencil->createDepth(_device.Get(), depthInfo);

		_descriptorHeapManager->createDepthStencilView(_depthStencil->getAdressOf(), &_dsv, 1);
	}

	//���[�g�V�O�l�`���̃T�|�[�g�o�[�W�������`�F�b�N
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	//�e�N�X�`���R�s�[�R�}���h�𔭍s����p�̃A���P�[�^�[�ƃ��X�g
	ComPtr<ID3D12Resource> uploadHeaps[4] = {};
	auto commandListSet = _commandContext->requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	//���_�o�b�t�@����
	{
		std::vector<Vertex> triangleVertices = {
			{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f } },
		{ { -0.25f, 0.25f, 0.0f }, { 0.0f, 0.0f } },
		{ { 0.25f, 0.25f, 0.0f }, { 1.0f, 0.0f } },
		{ { 0.25f, -0.25f, 0.0f }, { 1.0f, 1.0f } },
		};

		_vertexBuffer = new VertexBuffer();
		_vertexBuffer->createDeferred<Vertex>(_device.Get(), commandList, &uploadHeaps[0], triangleVertices);
	}

	//�C���f�b�N�X�o�b�t�@
	{
		std::vector<UINT32> indices = {
			0, 1, 2,
			0, 2, 3
		};

		_indexBuffer = new IndexBuffer();
		_indexBuffer->createDeferred(_device.Get(), commandList, &uploadHeaps[1], indices);
	}

	//�e�N�X�`���ǂݍ���
	_gpuResourceManager->createTextures(_device.Get(), *_commandContext, { "T_town_stone_D.dds", "T_town_stone_N.dds" });

	SharedMaterialCreateSettings materialSettings;
	materialSettings.name = "TestM";
	materialSettings.vertexShaderName = "shaders.hlsl";
	materialSettings.pixelShaderName = "shaders.hlsl";
	materialSettings.vsTextures = {};
	materialSettings.psTextures = { "T_town_stone_D.dds", "T_town_stone_N.dds" };
	_gpuResourceManager->createSharedMaterial(_device.Get(), materialSettings);
	_gpuResourceManager->loadSharedMaterial("TestM", _material);

	//�A�b�v���[�h�o�b�t�@��GPU�I�����[�o�b�t�@�ɃR�s�[
	_commandContext->executeCommandList(commandList);
	_commandContext->discardCommandListSet(commandListSet);

	//�R�s�[���I���܂ŃA�b�v���[�h�q�[�v��j�����Ȃ�
	_commandContext->waitForIdle();

	//�t���[�����\�[�X
	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i] = new FrameResource(_device.Get(), _swapChain.Get(), i);
	}

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = _frameResources[_frameIndex];
}

void GraphicsCore::onUpdate() {
	struct Vector4 {
		float x;
		float y;
		float z;
		float w;
	};

	struct Vector2 {
		float x;
		float y;
	};

	static Vector2 ff = { 2, 0 };
	ff.x += 0.05f;

	static Vector4 oo = { 0, 0, 0, 0 };
	oo.x += 0.01f;

	_material->setParameter<Vector4>("offset2", oo);
	_material->setParameter<Vector2>("offset2_2", ff);
}

void GraphicsCore::onRender() {
	auto commandListSet = _commandContext->requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	//�f�X�N���v�^�q�[�v���Z�b�g
	ID3D12DescriptorHeap* ppHeap[] = { _descriptorHeapManager->descriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
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

	//�}�e���A����`��
	RenderSettings renderSettings = { commandList };
	_material->setupRenderCommand(renderSettings);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &_vertexBuffer->_vertexBufferView);
	commandList->IASetIndexBuffer(&_indexBuffer->_indexBufferView);
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	//�`��p���\�[�X�o���A��W�J
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//�R�}���h�L���[�ɃR�}���h���X�g��n���Ď��s
	_commandContext->executeCommandList(commandListSet);
	_commandContext->discardCommandListSet(commandListSet);

	//��ʕ\��
	throwIfFailed(_swapChain->Present(1, 0));

	//�`�抮���܂őҋ@
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//�`�撆�ɔj�����Ă��܂��ƍ���̂ő҂�
	_commandContext->waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		delete _frameResources[i];
	}

	delete _commandContext;
	delete _gpuResourceManager;
	delete _descriptorHeapManager;
	delete _vertexBuffer;
	delete _indexBuffer;
	delete _depthStencil;

	_swapChain = nullptr;
	_device = nullptr;
}

void GraphicsCore::moveToNextFrame() {
	CommandQueue* commandQueue = _commandContext->commandQueue();
	const UINT64 currentFenceValue = _frameResources[_frameIndex]->_fenceValue;
	commandQueue->waitForFence(currentFenceValue);

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = _frameResources[_frameIndex];
	SharedMaterial::frameIndex = _frameIndex;

	const UINT64 fenceValue = commandQueue->incrementFence();
	_frameResources[_frameIndex]->_fenceValue = fenceValue;
}
