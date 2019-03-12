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

		//仮想Direct3D12対応GPUは無視する
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		//デバイス生成の確認が成功したら終了
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
			break;
		}
	}

	//デバイス生成
	throwIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device)));

	//デスクリプタヒープマネージャー
	_descriptorHeapManager = new DescriptorHeapManager();
	_descriptorHeapManager->create(_device.Get());

	//コマンドキュー生成
	_commandContext = new CommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	_commandContext->create(_device.Get());

	//スワップチェーン生成
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

	//フルスクリーンサポートをオフ
	throwIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
	throwIfFailed(swapChain.As(&_swapChain));

	//ビューポート初期化
	_viewPort = { 0.0f, 0.0f, static_cast<float>(_width), static_cast<float>(_height) };
	_scissorRect = { 0, 0, static_cast<LONG>(_width), static_cast<LONG>(_height) };

	//デプスバッファ生成
	{
		DepthTextureInfo depthInfo;
		depthInfo.width = _width;
		depthInfo.height = _height;
		depthInfo.format = DXGI_FORMAT_D32_FLOAT;
		_depthStencil = new Texture2D();
		_depthStencil->createDepth(_device.Get(), depthInfo);

		_descriptorHeapManager->createDepthStencilView(_depthStencil->getAdressOf(), &_dsv, 1);
	}

	//ルートシグネチャのサポートバージョンをチェック
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	//シェーダー＆パイプラインステート生成
	{
		VertexShader vertexShader;
		vertexShader.create("shaders.hlsl");

		PixelShader pixelShader;
		pixelShader.create("shaders.hlsl");

		_rootSignature = new RootSignature();
		_rootSignature->create(_device.Get(), vertexShader, pixelShader);

		_pipelineState = new PipelineState();
		_pipelineState->create(_device.Get(), _rootSignature, vertexShader, pixelShader); 
	}

	//テクスチャコピーコマンドを発行する用のアロケーターとリスト
	ComPtr<ID3D12Resource> uploadHeaps[4] = {};
	auto commandListSet = _commandContext->requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	//頂点バッファ生成
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

	//インデックスバッファ
	{
		std::vector<UINT32> indices = {
			0, 1, 2,
			0, 2, 3
		};

		_indexBuffer = new IndexBuffer();
		_indexBuffer->createDeferred(_device.Get(), commandList, &uploadHeaps[1], indices);
	}

	//テクスチャ
	{
		auto tmpTexP = generateTextureData(0);
		TextureInfo textureInfo;
		textureInfo.width = TextureWidth;
		textureInfo.height = TextureHeight;
		textureInfo.mipLevels = 1;
		textureInfo.dataPtr = tmpTexP.data();
		textureInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		_texture = new Texture2D();
		_texture->createDeferred(_device.Get(), commandList, &uploadHeaps[2], textureInfo);

		auto tmpTexP2 = generateTextureData(1);
		TextureInfo textureInfo2;
		textureInfo2.width = TextureWidth;
		textureInfo2.height = TextureHeight;
		textureInfo2.mipLevels = 1;
		textureInfo2.dataPtr = tmpTexP2.data();
		textureInfo2.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		_texture2 = new Texture2D();
		_texture2->createDeferred(_device.Get(), commandList, &uploadHeaps[3], textureInfo2);

		ID3D12Resource* textures[2] = { _texture->get(), _texture2->get() };

		//テクスチャのシェーダーリソースビューを生成
		_descriptorHeapManager->createShaderResourceView(textures, &_textureSrv, 2);
	}

	//アップロードバッファをGPUオンリーバッファにコピー
	_commandContext->executeCommandList(commandList);
	_commandContext->discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	_commandContext->waitForIdle();

	//フレームリソース
	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i] = new FrameResource(_device.Get(), _swapChain.Get(), i);
	}

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = _frameResources[_frameIndex];
}

void GraphicsCore::onUpdate() {
	_constantBufferData.offset[0] = _constantBufferData.offset[0] + 0.01f;
	_currentFrameResource->writeConstantBuffer(_constantBufferData);
}

void GraphicsCore::onRender() {
	auto commandListSet = _commandContext->requestCommandListSet(_pipelineState->_pipelineState.Get());
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	//デスクリプタヒープをセット
	ID3D12DescriptorHeap* ppHeap[] = { _descriptorHeapManager->descriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	commandList->SetDescriptorHeaps(1, ppHeap);

	//ビューポート設定
	commandList->RSSetViewports(1, &_viewPort);
	commandList->RSSetScissorRects(1, &_scissorRect);

	//コマンド積む用のリソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//レンダーターゲット・デプスステンシルバッファをセット
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _currentFrameResource->_rtv->cpuHandle;
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &_dsv->cpuHandle);

	//レンダーターゲットクリア
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(_dsv->cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//描画コマンドを登録
	commandList->SetGraphicsRootSignature(_rootSignature->_rootSignature.Get());

	//デスクリプタヒープにセットした定数バッファをセット
	commandList->SetGraphicsRootDescriptorTable(0, _textureSrv->gpuHandle);
	commandList->SetGraphicsRootDescriptorTable(1, _currentFrameResource->_sceneCbv->gpuHandle);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &_vertexBuffer->_vertexBufferView);
	commandList->IASetIndexBuffer(&_indexBuffer->_indexBufferView);
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	//描画用リソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//コマンドキューにコマンドリストを渡して実行
	_commandContext->executeCommandList(commandListSet);
	_commandContext->discardCommandListSet(commandListSet);

	//画面表示
	throwIfFailed(_swapChain->Present(1, 0));

	//描画完了まで待機
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//描画中に破棄してしまうと困るので待つ
	_commandContext->waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		delete _frameResources[i];
	}

	delete _commandContext;
	delete _descriptorHeapManager;
	delete _vertexBuffer;
	delete _indexBuffer;
	delete _texture;
	delete _texture2;
	delete _depthStencil;
	delete _pipelineState;
	delete _rootSignature;

	_swapChain = nullptr;
	_device = nullptr;
}

void GraphicsCore::moveToNextFrame() {
	CommandQueue* commandQueue = _commandContext->commandQueue();
	const UINT64 currentFenceValue = _frameResources[_frameIndex]->_fenceValue;
	commandQueue->waitForFence(currentFenceValue);

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = _frameResources[_frameIndex];

	const UINT64 fenceValue = commandQueue->incrementFence();
	_frameResources[_frameIndex]->_fenceValue = fenceValue;
}
