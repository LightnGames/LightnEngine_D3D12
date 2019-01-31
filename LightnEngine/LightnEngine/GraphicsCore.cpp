#include "GraphicsCore.h"
#include <d3dcompiler.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "FrameResource.h"
#include "CommandQueue.h"
#include "CommandContext.h"
#include "DescriptorHeap.h"

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
		D3D12_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		dsDesc.StencilEnable = FALSE;
		dsDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		dsDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		D3D12_DEPTH_STENCILOP_DESC dsopDesc = {};
		dsopDesc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		dsopDesc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		dsopDesc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		dsopDesc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		dsDesc.FrontFace = dsopDesc;
		dsDesc.BackFace = dsopDesc;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		D3D12_RESOURCE_DESC depthBufferDesc = {};
		depthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthBufferDesc.Alignment = 0;
		depthBufferDesc.Width = _width;
		depthBufferDesc.Height = _height;
		depthBufferDesc.DepthOrArraySize = 1;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		_device->CreateCommittedResource(&LTND3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthBufferDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&_depthStencil));
		NAME_D3D12_OBJECT(_depthStencil);

		_descriptorHeapManager->createDepthStencilView(_depthStencil.GetAddressOf(), &_dsv, 1);
	}

	//ルートシグネチャのサポートバージョンをチェック
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}


	//サンプラーを定義してルートシグネチャを生成
	{
		D3D12_DESCRIPTOR_RANGE1 srvRangeDesc[1] = {};
		srvRangeDesc[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRangeDesc[0].NumDescriptors = 1;
		srvRangeDesc[0].BaseShaderRegister = 0;
		srvRangeDesc[0].RegisterSpace = 0;
		srvRangeDesc[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		srvRangeDesc[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 cbvRangeDesc[1] = {};
		cbvRangeDesc[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangeDesc[0].NumDescriptors = 1;
		cbvRangeDesc[0].BaseShaderRegister = 0;
		cbvRangeDesc[0].RegisterSpace = 0;
		cbvRangeDesc[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		cbvRangeDesc[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 parameterDesc[2] = {};
		parameterDesc[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDesc[0].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[0].DescriptorTable.pDescriptorRanges = &srvRangeDesc[0];

		parameterDesc[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDesc[1].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[1].DescriptorTable.pDescriptorRanges = &cbvRangeDesc[0];

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSignatureDesc.Desc_1_1.NumParameters = 2;
		rootSignatureDesc.Desc_1_1.pParameters = &parameterDesc[0];
		rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
		rootSignatureDesc.Desc_1_1.pStaticSamplers = &samplerDesc;
		rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		throwIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
		throwIfFailed(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));
		NAME_D3D12_OBJECT(_rootSignature);
	}

	//シェーダー生成
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

		throwIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vertexShader, nullptr));
		throwIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &pixelShader, nullptr));

		D3D12_INPUT_ELEMENT_DESC inputElementDscs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_SHADER_BYTECODE vsByteCode = {};
		vsByteCode.BytecodeLength = vertexShader->GetBufferSize();
		vsByteCode.pShaderBytecode = vertexShader->GetBufferPointer();

		D3D12_SHADER_BYTECODE psByteCode = {};
		psByteCode.BytecodeLength = pixelShader->GetBufferSize();
		psByteCode.pShaderBytecode = pixelShader->GetBufferPointer();

		//パイプラインステート生成
		D3D12_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.DepthClipEnable = TRUE;
		rasterizerDesc.MultisampleEnable = FALSE;
		rasterizerDesc.AntialiasedLineEnable = FALSE;
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;

		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
			FALSE, FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};

		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
			blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDscs, _ARRAYSIZE(inputElementDscs) };
		psoDesc.pRootSignature = _rootSignature.Get();
		psoDesc.VS = vsByteCode;
		psoDesc.PS = psByteCode;
		psoDesc.RasterizerState = rasterizerDesc;
		psoDesc.BlendState = blendDesc;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		throwIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pipelineState)));
		NAME_D3D12_OBJECT(_pipelineState);

	}

	//頂点バッファ生成
	{
		Vertex triangleVertices[] = {
			{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f } },
		{ { -0.25f, 0.25f, 0.0f }, { 0.0f, 0.0f } },
		{ { 0.25f, 0.25f, 0.0f }, { 1.0f, 0.0f } },
		{ { 0.25f, -0.25f, 0.0f }, { 1.0f, 1.0f } },
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		D3D12_HEAP_PROPERTIES vertexHeapProperties = {};
		vertexHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC vertexBufferDesc = {};
		vertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		vertexBufferDesc.Alignment = 0;
		vertexBufferDesc.Width = vertexBufferSize;
		vertexBufferDesc.Height = 1;
		vertexBufferDesc.DepthOrArraySize = 1;
		vertexBufferDesc.MipLevels = 1;
		vertexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		vertexBufferDesc.SampleDesc.Count = 1;
		vertexBufferDesc.SampleDesc.Quality = 0;
		vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		vertexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		throwIfFailed(_device->CreateCommittedResource(
			&vertexHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&_vertexBuffer)
		));
		NAME_D3D12_OBJECT(_vertexBuffer);

		//生成した頂点バッファにデータをコピー
		UINT8* pVertexDataBegin;
		D3D12_RANGE mapRange = {};
		throwIfFailed(_vertexBuffer->Map(0, &mapRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		_vertexBuffer->Unmap(0, nullptr);

		_vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
		_vertexBufferView.StrideInBytes = sizeof(Vertex);
		_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	//インデックスバッファ
	{
		UINT32 indices[] = {
			0, 1, 2,
			0, 2, 3
		};

		const UINT indexBufferSize = sizeof(indices);

		D3D12_HEAP_PROPERTIES indexHeapProperties = {};
		indexHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC indexBufferDesc = {};
		indexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		indexBufferDesc.Alignment = 0;
		indexBufferDesc.Width = indexBufferSize;
		indexBufferDesc.Height = 1;
		indexBufferDesc.DepthOrArraySize = 1;
		indexBufferDesc.MipLevels = 1;
		indexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		indexBufferDesc.SampleDesc.Count = 1;
		indexBufferDesc.SampleDesc.Quality = 0;
		indexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		indexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		throwIfFailed(_device->CreateCommittedResource(
			&indexHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&_indexBuffer)
		));
		NAME_D3D12_OBJECT(_indexBuffer);

		//生成した頂点バッファにデータをコピー
		UINT8* pIndexDataBegin;
		D3D12_RANGE mapRange = {};
		throwIfFailed(_indexBuffer->Map(0, &mapRange, reinterpret_cast<void**>(&pIndexDataBegin)));
		memcpy(pIndexDataBegin, indices, indexBufferSize);
		_indexBuffer->Unmap(0, nullptr);

		_indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
		_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		_indexBufferView.SizeInBytes = indexBufferSize;
	}

	//テクスチャコピーコマンドを発行する用のアロケーターとリスト
	auto commandListSet = _commandContext->requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	//ここにテクスチャアップロード用ヒープを置いておかないとコマンドリストを実行する前にDeleteされて面倒なことに
	ComPtr<ID3D12Resource> textureUploadHeap;

	//テクスチャ生成
	{
		//GPU読み出し専用テクスチャ本体を生成
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		textureDesc.Width = TextureWidth;
		textureDesc.Height = TextureHeight;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		throwIfFailed(_device->CreateCommittedResource(&LTND3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_texture)));

		UINT64 requiredSize = 0;
		_device->GetCopyableFootprints(&_texture->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);

		//テクスチャにデータをセットするためにCPUからも書き込み可能なバッファを生成
		D3D12_RESOURCE_DESC textureBufferDesc = {};
		textureBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		textureBufferDesc.Alignment = 0;
		textureBufferDesc.Width = requiredSize;
		textureBufferDesc.Height = 1;
		textureBufferDesc.DepthOrArraySize = 1;
		textureBufferDesc.MipLevels = 1;
		textureBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		textureBufferDesc.SampleDesc.Count = 1;
		textureBufferDesc.SampleDesc.Quality = 0;
		textureBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		textureBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		throwIfFailed(_device->CreateCommittedResource(&LTND3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &textureBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&textureUploadHeap)));
		NAME_D3D12_OBJECT(_texture);

		//テクスチャデータをセット
		std::vector<UINT8> texture = generateTextureData();
		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &texture[0];
		textureData.RowPitch = TextureWidth * TexturePixelSize;
		textureData.SlicePitch = textureData.RowPitch*TextureHeight;

		//GPU読み出し専用バッファにコピーコマンドを発行
		updateSubresources(commandList, _texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		//テクスチャのシェーダーリソースビューを生成
		_descriptorHeapManager->createShaderResourceView(_texture.GetAddressOf(), &_textureSrv, 1);

		//GPUにコマンドを発行するのでいったんクローズ
		//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
		_commandContext->executeCommandList(commandList);
		_commandContext->discardCommandListSet(commandListSet);
	}

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
	auto commandListSet = _commandContext->requestCommandListSet(_pipelineState.Get());
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;
	commandList->SetGraphicsRootSignature(_rootSignature.Get());

	//デスクリプタヒープをセット
	ID3D12DescriptorHeap* ppHeap[] = { _descriptorHeapManager->descriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	commandList->SetDescriptorHeaps(1, ppHeap);

	//デスクリプタヒープにセットした定数バッファをセット
	commandList->SetGraphicsRootDescriptorTable(0, _textureSrv->gpuHandle);
	commandList->SetGraphicsRootDescriptorTable(1, _currentFrameResource->_sceneCbv->gpuHandle);

	commandList->RSSetViewports(1, &_viewPort);
	commandList->RSSetScissorRects(1, &_scissorRect);

	//コマンド積む用のリソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//レンダーターゲット・デプスステンシルバッファをセット
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _currentFrameResource->_rtv->cpuHandle;
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &_dsv->cpuHandle);

	//描画コマンドを登録
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(_dsv->cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &_vertexBufferView);
	commandList->IASetIndexBuffer(&_indexBufferView);
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	//描画用リソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

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
	_commandContext->commandQueue()->waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		delete _frameResources[i];
	}

	delete _commandContext;
	delete _descriptorHeapManager;

	_swapChain = nullptr;
	_device = nullptr;
	_depthStencil = nullptr;
	_rootSignature = nullptr;
	_pipelineState = nullptr;

	_vertexBuffer = nullptr;
	_indexBuffer = nullptr;
	_texture = nullptr;
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
