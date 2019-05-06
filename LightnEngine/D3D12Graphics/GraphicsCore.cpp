#include "GraphicsCore.h"
#include <stdexcept>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "SharedMaterial.h"
#include "RenderableEntity.h"

#include "ThirdParty/Imgui/imgui.h"

RefPtr<SharedMaterial> mat;
RefPtr<VertexAndIndexBuffer> viBuffer;
ComPtr<ID3D12CommandSignature> m_commandSignature;
ComPtr<ID3D12Resource> in_commandBuffer[FrameCount];
ComPtr<ID3D12Resource> out_commandBuffer[FrameCount];

ComPtr<ID3D12Resource> in_processedCommandBuffer;
ComPtr<ID3D12Resource> out_processedCommandBuffer[FrameCount];
ComPtr<ID3D12Resource> out_processedCommandBufferCounterReset;
BufferView out_processedUavView[FrameCount];

ComPtr<ID3D12RootSignature> _cullingComputeRootSignature;
ComPtr<ID3D12PipelineState> _cullingComputeState;

ComPtr<ID3D12RootSignature> _setupCommandComputeRootSignature;
ComPtr<ID3D12PipelineState> _setupCommandComputeState;
BufferView setupCommandUavView[FrameCount];

ConstantBufferMaterial gpuCullingCameraInfo;

bool gpuDrivenStenby = false;

struct IndirectCommand
{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct ObjectInfo {
	Matrix4 mtxWorld;
	Vector3 startPosAABB;
	Vector3 endposAABB;
	Color color;
};

struct PerInstanceIndirect {
	Matrix4 mtxWorld;
	Color color;
};

constexpr UINT TriangleCount = 128;
constexpr UINT TriangleBufferSize = TriangleCount * sizeof(PerInstanceIndirect);
constexpr UINT CommandBufferCounterOffset = AlignForUavCounter(TriangleCount * sizeof(PerInstanceIndirect));
constexpr UINT CommandBufferCounterOffset2 = AlignForUavCounter(sizeof(IndirectCommand));

GraphicsCore::GraphicsCore() :
	_width(1280),
	_height(720),
	_frameIndex(0),
	_viewPort({}),
	_scissorRect({}),
	_dsv(),
	_currentFrameResource(nullptr) {
}

GraphicsCore::~GraphicsCore() {
}

void GraphicsCore::onInit(HWND hwnd) {
	UINT dxgiFactoryFlags = 0;
#ifdef DEBUG
	ComPtr<ID3D12Debug> debugController;
	ComPtr<ID3D12Debug1> debugControllerGpu;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		debugController->QueryInterface(IID_PPV_ARGS(&debugControllerGpu));
		//debugControllerGpu->SetEnableGPUBasedValidation(true);
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
	_descriptorHeapManager.create(_device.Get());

	//コマンドキュー生成
	_commandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

	//Imgui初期化
	_imguiWindow.init(hwnd, _device.Get());

	//デバッグ描画機能初期化
	_debugGeometryRender.create(_device.Get(), &_commandContext);

	//GPUコマンド格納アロケータ初期化
	_gpuCommandArray.init(163840);

	//スワップチェーン生成
	{
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


		//フルスクリーンサポートをオフ
		throwIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		throwIfFailed(swapChain.As(&_swapChain));
	}

	//ビューポート初期化
	_viewPort.TopLeftX = 0.0f;
	_viewPort.TopLeftY = 0.0f;
	_viewPort.Width = static_cast<float>(_width);
	_viewPort.Height = static_cast<float>(_height);
	_viewPort.MinDepth = 0.0f;
	_viewPort.MaxDepth = 1.0f;

	_scissorRect = { 0, 0, static_cast<LONG>(_width), static_cast<LONG>(_height) };

	//デプスバッファ生成
	{
		DepthTextureInfo depthInfo;
		depthInfo.width = _width;
		depthInfo.height = _height;
		depthInfo.format = DepthStencilFormat;
		_depthStencil.createDepth(_device.Get(), depthInfo);

		_descriptorHeapManager.createDepthStencilView(_depthStencil.getAdressOf(), &_dsv, 1);
	}

	//フレームリソース
	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].create(_device.Get(), _swapChain.Get(), i);
	}

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];

	//コマンドシグネチャ生成
	{
		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
			{ "POSITION",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
			{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};

		_gpuResourceManager.createVertexAndIndexBuffer(_device.Get(), _commandContext, { "cube.mesh" });
		_gpuResourceManager.loadVertexAndIndexBuffer("cube.mesh", &viBuffer);

		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "TestI";
		materialSettings.vertexShaderName = "Indirect.hlsl";
		materialSettings.pixelShaderName = "Indirect.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = {};
		materialSettings.inputLayouts = inputLayouts;
		materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		_gpuResourceManager.createSharedMaterial(_device.Get(), materialSettings);
		_gpuResourceManager.loadSharedMaterial("TestI", &mat);

		// Each command consists of a CBV update and a DrawInstanced call.
		D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2] = {};
		argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
		argumentDescs[0].VertexBuffer.Slot = 1;
		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
		commandSignatureDesc.pArgumentDescs = argumentDescs;
		commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
		commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

		throwIfFailed(_device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&m_commandSignature)));
		NAME_D3D12_OBJECT(m_commandSignature);
	}

	//GPUカリングステート
	D3D12_HEAP_PROPERTIES defaultHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };
	D3D12_HEAP_PROPERTIES uploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
	{
		gpuCullingCameraInfo.create(_device.Get(), { sizeof(SceneConstant) });

		D3D12_DESCRIPTOR_RANGE1 uavRange = {};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = 1;
		uavRange.BaseShaderRegister = 0;
		uavRange.RegisterSpace = 0;
		uavRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 parameterDesc[3] = {};
		parameterDesc[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameterDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[0].Descriptor.ShaderRegister = 0;
		parameterDesc[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDesc[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[1].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[1].DescriptorTable.pDescriptorRanges = &uavRange;

		parameterDesc[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDesc[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[2].Descriptor.ShaderRegister = 0;
		parameterDesc[2].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSignatureDesc.Desc_1_1.NumParameters = 3;
		rootSignatureDesc.Desc_1_1.pParameters = parameterDesc;
		rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
		rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		throwIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
		throwIfFailed(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_cullingComputeRootSignature)));
		NAME_D3D12_OBJECT(_cullingComputeRootSignature);

		ComPtr<ID3DBlob> cullingComputeShader;
		ComPtr<ID3DBlob> cullingShaderError;
		throwIfFailed(D3DCompileFromFile(L"Shaders/GpuCulling_cs.hlsl", nullptr, nullptr, "CSMain", "cs_5_0", 0, 0, &cullingComputeShader, &cullingShaderError));

		D3D12_SHADER_BYTECODE cullingShaderByteCode;
		cullingShaderByteCode.pShaderBytecode = cullingComputeShader->GetBufferPointer();
		cullingShaderByteCode.BytecodeLength = cullingComputeShader->GetBufferSize();

		// Describe and create the compute pipeline state object (PSO).
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = _cullingComputeRootSignature.Get();
		computePsoDesc.CS = cullingShaderByteCode;

		throwIfFailed(_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&_cullingComputeState)));
		NAME_D3D12_OBJECT(_cullingComputeState);
	}

	//IndirectCommandステート
	{
		D3D12_DESCRIPTOR_RANGE1 uavRange = {};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = 1;
		uavRange.BaseShaderRegister = 0;
		uavRange.RegisterSpace = 0;
		uavRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER1 parameterDesc[3] = {};
		parameterDesc[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameterDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[0].Descriptor.ShaderRegister = 0;
		parameterDesc[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

		parameterDesc[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameterDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[1].Descriptor.ShaderRegister = 1;
		parameterDesc[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

		parameterDesc[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[2].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[2].DescriptorTable.pDescriptorRanges = &uavRange;

		//parameterDesc[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		//parameterDesc[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		//parameterDesc[3].Descriptor.ShaderRegister = 0;
		//parameterDesc[3].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSignatureDesc.Desc_1_1.NumParameters = 3;
		rootSignatureDesc.Desc_1_1.pParameters = parameterDesc;
		rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
		rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		throwIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
		throwIfFailed(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_setupCommandComputeRootSignature)));
		NAME_D3D12_OBJECT(_setupCommandComputeRootSignature);

		ComPtr<ID3DBlob> setupCommandComputeShader;
		ComPtr<ID3DBlob> setupCommandShaderError;
		throwIfFailed(D3DCompileFromFile(L"Shaders/SetupIndirectCommand_cs.hlsl", nullptr, nullptr, "CSMain", "cs_5_0", 0, 0, &setupCommandComputeShader, &setupCommandShaderError));

		D3D12_SHADER_BYTECODE setupCommandByteCode;
		setupCommandByteCode.pShaderBytecode = setupCommandComputeShader->GetBufferPointer();
		setupCommandByteCode.BytecodeLength = setupCommandComputeShader->GetBufferSize();

		// Describe and create the compute pipeline state object (PSO).
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = _setupCommandComputeRootSignature.Get();
		computePsoDesc.CS = setupCommandByteCode;

		throwIfFailed(_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&_setupCommandComputeState)));
		NAME_D3D12_OBJECT(_setupCommandComputeState);
	}


	for (uint32 i = 0; i < FrameCount; ++i) {
		D3D12_RESOURCE_DESC outCommandBufferDesc = {};
		outCommandBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		outCommandBufferDesc.Width = CommandBufferCounterOffset + sizeof(UINT);
		outCommandBufferDesc.Height = 1;
		outCommandBufferDesc.DepthOrArraySize = 1;
		outCommandBufferDesc.MipLevels = 1;
		outCommandBufferDesc.SampleDesc.Count = 1;
		outCommandBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		outCommandBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		throwIfFailed(_device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&outCommandBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&out_processedCommandBuffer[i])));
	}

	NAME_D3D12_OBJECT_INDEXED(out_processedCommandBuffer, FrameCount);

	ComPtr<ID3D12Resource> inCommandUploadBuffer;
	D3D12_RESOURCE_DESC inCommandBufferDesc = {};
	inCommandBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	inCommandBufferDesc.Width = TriangleCount * sizeof(ObjectInfo);
	inCommandBufferDesc.Height = 1;
	inCommandBufferDesc.DepthOrArraySize = 1;
	inCommandBufferDesc.MipLevels = 1;
	inCommandBufferDesc.SampleDesc.Count = 1;
	inCommandBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	throwIfFailed(_device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&inCommandBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&inCommandUploadBuffer)));

	throwIfFailed(_device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&inCommandBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&in_processedCommandBuffer)));

	NAME_D3D12_OBJECT(in_processedCommandBuffer);

	auto commandListSet = _commandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	VectorArray<ObjectInfo> objectInfos(TriangleCount);
	for (size_t i = 0; i < objectInfos.size(); ++i) {
		Vector3 position(i * 1.2f - TriangleCount / 2, 0, 2);
		objectInfos[i].mtxWorld = Matrix4::translateXYZ(position).transpose();
		objectInfos[i].startPosAABB = position;
		objectInfos[i].color = Color(i * 0.01f, 0, 0, 1);
	}

	ObjectInfo* mapPtrC = nullptr;
	inCommandUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapPtrC));
	memcpy(mapPtrC, objectInfos.data(), sizeof(ObjectInfo) * TriangleCount);

	commandList->CopyBufferRegion(in_processedCommandBuffer.Get(), 0, inCommandUploadBuffer.Get(), 0, TriangleCount * sizeof(ObjectInfo));
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(in_processedCommandBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = TriangleCount;
		uavDesc.Buffer.StructureByteStride = sizeof(PerInstanceIndirect);
		uavDesc.Buffer.CounterOffsetInBytes = CommandBufferCounterOffset;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		for (uint32 i = 0; i < FrameCount; ++i) {
			_descriptorHeapManager.getDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->allocateBufferView(&out_processedUavView[i], 1);

			_device->CreateUnorderedAccessView(
				out_processedCommandBuffer[i].Get(),
				out_processedCommandBuffer[i].Get(),
				&uavDesc,
				out_processedUavView[i].cpuHandle);
		}
	}

	VectorArray<IndirectCommand> commands(FrameCount);
	ComPtr<ID3D12Resource> indirectCommandUpload[FrameCount];

	for (uint32 i = 0; i < FrameCount; ++i) {
		{
			D3D12_RESOURCE_DESC bufferDesc = {};
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufferDesc.Width = CommandBufferCounterOffset2 + sizeof(UINT);
			bufferDesc.Height = 1;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.MipLevels = 1;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			throwIfFailed(_device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&out_commandBuffer[i])));
		}

		{
			D3D12_RESOURCE_DESC bufferDesc = {};
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufferDesc.Width = sizeof(IndirectCommand);
			bufferDesc.Height = 1;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.MipLevels = 1;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			throwIfFailed(_device->CreateCommittedResource(
				&uploadHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&indirectCommandUpload[i])));

			throwIfFailed(_device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&in_commandBuffer[i])));


			D3D12_VERTEX_BUFFER_VIEW uavVertexBufferView;
			uavVertexBufferView.BufferLocation = out_processedCommandBuffer[i]->GetGPUVirtualAddress();
			uavVertexBufferView.StrideInBytes = sizeof(PerInstanceIndirect);
			uavVertexBufferView.SizeInBytes = CommandBufferCounterOffset + sizeof(UINT);

			commands[i].vertexBufferView = uavVertexBufferView;
			commands[i].drawArguments.IndexCountPerInstance = viBuffer->indexBuffer._indexCount;
			commands[i].drawArguments.InstanceCount = TriangleCount;
			commands[i].drawArguments.BaseVertexLocation = 0;
			commands[i].drawArguments.StartIndexLocation = 0;
			commands[i].drawArguments.StartInstanceLocation = 0;

			IndirectCommand* mapPtr = nullptr;
			indirectCommandUpload[i]->Map(0, nullptr, reinterpret_cast<void**>(&mapPtr));
			memcpy(mapPtr, &commands[i], sizeof(IndirectCommand));

			commandList->CopyBufferRegion(in_commandBuffer[i].Get(), 0, indirectCommandUpload[i].Get(), 0, sizeof(IndirectCommand));
			commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(in_commandBuffer[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
			//カッコカリ
			//commandList->CopyBufferRegion(out_commandBuffer[i].Get(), 0, indirectCommandUpload[i].Get(), 0, sizeof(IndirectCommand));
		}
	}

	NAME_D3D12_OBJECT_INDEXED(out_commandBuffer, FrameCount);

	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = 10;
		uavDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
		uavDesc.Buffer.CounterOffsetInBytes = CommandBufferCounterOffset2;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		for (uint32 i = 0; i < FrameCount; ++i) {
			_descriptorHeapManager.getDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->allocateBufferView(&setupCommandUavView[i], 1);

			_device->CreateUnorderedAccessView(
				out_commandBuffer[i].Get(),
				out_commandBuffer[i].Get(),
				&uavDesc,
				setupCommandUavView[i].cpuHandle);
		}
	}

	//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
	_commandContext.executeCommandList(commandList);
	_commandContext.discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	_commandContext.waitForIdle();

	//return;

	//UAVカウンタをリセットするための(UINT)0のバッファを生成
	D3D12_RESOURCE_DESC countBufferDesc = {};
	countBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	countBufferDesc.Width = sizeof(UINT);
	countBufferDesc.Height = 1;
	countBufferDesc.DepthOrArraySize = 1;
	countBufferDesc.MipLevels = 1;
	countBufferDesc.SampleDesc.Count = 1;
	countBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	throwIfFailed(_device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&countBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&out_processedCommandBufferCounterReset)));

	UINT8* pMappedCounterReset = nullptr;
	throwIfFailed(out_processedCommandBufferCounterReset->Map(0, nullptr, reinterpret_cast<void**>(&pMappedCounterReset)));
	ZeroMemory(pMappedCounterReset, sizeof(UINT));
	out_processedCommandBufferCounterReset->Unmap(0, nullptr);

	gpuDrivenStenby = true;
}

void GraphicsCore::onUpdate() {
	_imguiWindow.startFrame();

	static Vector3 positionC = -Vector3::forward * 10;
	static float pitchC = 0;
	static float yawC = 0;
	static float rollC = 0;
	static float fov = 60;
	static float farZ = 1000;
	static float nearZ = 0.01f;

	ImGui::Begin("Camera");
	ImGui::DragFloat3("Position", (float*)& positionC, 0.05f);
	ImGui::SliderAngle("Picth", &pitchC);
	ImGui::SliderAngle("Yaw", &yawC);
	ImGui::SliderAngle("Roll", &rollC);
	ImGui::SliderFloat("Fov", &fov, 0, 120);
	ImGui::SliderFloat("NearZ", &nearZ, 0.001f, 10);
	ImGui::SliderFloat("FarZ", &farZ, 10, 1000);
	ImGui::End();

	RefPtr<Camera> mainCamera = _gpuResourceManager.getMainCamera();
	mainCamera->setPosition(positionC);
	mainCamera->setRotationEuler(pitchC, yawC, rollC, true);
	mainCamera->setFieldOfView(fov);
	mainCamera->setNearZ(nearZ);
	mainCamera->setFarZ(farZ);
	mainCamera->setAspectRate(_width, _height);
	mainCamera->computeProjectionMatrix();
	mainCamera->computeViewMatrix();

	static Vector3 positionV = -Vector3::forward * 5;
	static float pitchV = 0;
	static float yawV = 0;
	static float rollV = 0;
	static float fovV = 60;
	static float farZV = 10;
	static float nearZV = 0.5f;

	ImGui::Begin("Virtual Camera");
	ImGui::DragFloat3("Position", (float*)& positionV, 0.05f);
	ImGui::SliderAngle("Picth", &pitchV);
	ImGui::SliderAngle("Yaw", &yawV);
	ImGui::SliderAngle("Roll", &rollV);
	ImGui::SliderFloat("Fov", &fovV, 0, 120);
	ImGui::SliderFloat("NearZ", &nearZV, 0.001f, 10);
	ImGui::SliderFloat("FarZ", &farZV, 10, 1000);
	ImGui::End();

	Quaternion virtualRotate = Quaternion::euler({ pitchV,yawV,rollV }, true);
	Matrix4 virtualView = Matrix4::createWorldMatrix(positionV, virtualRotate, Vector3::one);
	Matrix4 virtualProj = Matrix4::perspectiveFovLH(radianFromDegree(fovV), _width / static_cast<float>(_height), nearZV, farZV);
	float x = 1 / virtualProj[0][0];
	float y = 1 / virtualProj[1][1];
	Color lineColor = Color::blue;

	Vector3 forwardNear = Quaternion::rotVector(virtualRotate, Vector3::forward);
	Vector3 topLeft = Quaternion::rotVector(virtualRotate, Vector3(-x, y, 1));
	Vector3 topRight = Quaternion::rotVector(virtualRotate, Vector3(x, y, 1));
	Vector3 bottomLeft = Quaternion::rotVector(virtualRotate, Vector3(-x, -y, 1));
	Vector3 bottomRight = Quaternion::rotVector(virtualRotate, Vector3(x, -y, 1));

	Vector3 farTopLeft = topLeft * farZV + positionV;
	Vector3 farTopRight = topRight * farZV + positionV;
	Vector3 farBottomLeft = bottomLeft * farZV + positionV;
	Vector3 farBottomRight = bottomRight * farZV + positionV;

	Vector3 nearTopLeft = topLeft * nearZV + positionV;
	Vector3 nearTopRight = topRight * nearZV + positionV;
	Vector3 nearBottomLeft = bottomLeft * nearZV + positionV;
	Vector3 nearBottomRight = bottomRight * nearZV + positionV;

	_debugGeometryRender.debugDrawLine(positionV, farTopLeft, lineColor);
	_debugGeometryRender.debugDrawLine(positionV, farTopRight, lineColor);
	_debugGeometryRender.debugDrawLine(positionV, farBottomLeft, lineColor);
	_debugGeometryRender.debugDrawLine(positionV, farBottomRight, lineColor);

	_debugGeometryRender.debugDrawLine(farTopRight, farTopLeft, lineColor);
	_debugGeometryRender.debugDrawLine(farTopLeft, farBottomLeft, lineColor);
	_debugGeometryRender.debugDrawLine(farBottomRight, farBottomLeft, lineColor);
	_debugGeometryRender.debugDrawLine(farBottomRight, farTopRight, lineColor);

	_debugGeometryRender.debugDrawLine(nearTopRight, nearTopLeft, lineColor);
	_debugGeometryRender.debugDrawLine(nearTopLeft, nearBottomLeft, lineColor);
	_debugGeometryRender.debugDrawLine(nearBottomRight, nearBottomLeft, lineColor);
	_debugGeometryRender.debugDrawLine(nearBottomRight, nearTopRight, lineColor);

	Vector3 leftNormal = Vector3::cross(Vector3(-x, 0, 1), -Vector3::up).normalize();
	Vector3 rightNormal = Vector3::cross(Vector3(x, 0, 1), Vector3::up).normalize();
	Vector3 bottomNormal = Vector3::cross(Vector3(0, y, 1), -Vector3::right).normalize();
	Vector3 topNormal = Vector3::cross(Vector3(0, -y, 1), Vector3::right).normalize();
	leftNormal = Quaternion::rotVector(virtualRotate, leftNormal);
	rightNormal = Quaternion::rotVector(virtualRotate, rightNormal);
	bottomNormal = Quaternion::rotVector(virtualRotate, bottomNormal);
	topNormal = Quaternion::rotVector(virtualRotate, topNormal);
	_debugGeometryRender.debugDrawLine(positionV, positionV + leftNormal, Color::yellow);
	_debugGeometryRender.debugDrawLine(positionV, positionV + rightNormal, Color::yellow);
	_debugGeometryRender.debugDrawLine(positionV, positionV + bottomNormal, Color::yellow);
	_debugGeometryRender.debugDrawLine(positionV, positionV + topNormal, Color::yellow);


	gpuCullingConstant.frustumPlanes[0] = leftNormal;
	gpuCullingConstant.frustumPlanes[1] = rightNormal;
	gpuCullingConstant.frustumPlanes[2] = bottomNormal;
	gpuCullingConstant.frustumPlanes[3] = topNormal;
	gpuCullingConstant.cameraPosition = positionV;

	if (gpuDrivenStenby) {
		gpuCullingCameraInfo.writeBufferData(&gpuCullingConstant, sizeof(gpuCullingConstant), 0);
		gpuCullingCameraInfo.flashBufferData(_frameIndex);
	}
}

void GraphicsCore::onRender() {
	if (gpuDrivenStenby) {
		auto computeCommandListSet = _commandContext.requestCommandListSet();
		RefPtr<ID3D12GraphicsCommandList> computeCommandList = computeCommandListSet.commandList;

		//デスクリプタヒープをセット
		RefPtr<ID3D12DescriptorHeap> ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		computeCommandList->SetDescriptorHeaps(1, ppHeap);

		//GPUカリング
		{
			computeCommandList->SetPipelineState(_cullingComputeState.Get());
			computeCommandList->SetComputeRootSignature(_cullingComputeRootSignature.Get());

			computeCommandList->SetComputeRootShaderResourceView(0, in_processedCommandBuffer->GetGPUVirtualAddress());
			computeCommandList->SetComputeRootDescriptorTable(1, out_processedUavView[_frameIndex].gpuHandle);
			computeCommandList->SetComputeRootConstantBufferView(2, gpuCullingCameraInfo.constantBuffers[_frameIndex][0]->_resource->GetGPUVirtualAddress());

			//AppendStructuredBufferのカウンタを0にリセットする
			computeCommandList->CopyBufferRegion(out_processedCommandBuffer[_frameIndex].Get(), CommandBufferCounterOffset, out_processedCommandBufferCounterReset.Get(), 0, sizeof(UINT));

			computeCommandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(out_processedCommandBuffer[_frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

			computeCommandList->Dispatch(1, 1, 1);
		}

		//ExecuteIndirectコマンド構築
		{
			computeCommandList->SetPipelineState(_setupCommandComputeState.Get());
			computeCommandList->SetComputeRootSignature(_setupCommandComputeRootSignature.Get());

			computeCommandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(out_processedCommandBuffer[_frameIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

			computeCommandList->SetComputeRootShaderResourceView(0, out_processedCommandBuffer[_frameIndex]->GetGPUVirtualAddress());
			computeCommandList->SetComputeRootShaderResourceView(1, in_commandBuffer[_frameIndex]->GetGPUVirtualAddress());
			computeCommandList->SetComputeRootDescriptorTable(2, setupCommandUavView[_frameIndex].gpuHandle);
			////computeCommandList->SetComputeRootConstantBufferView(3, gpuCullingCameraInfo.constantBuffers[_frameIndex][0]->_resource->GetGPUVirtualAddress());

			////AppendStructuredBufferのカウンタを0にリセットする
			computeCommandList->CopyBufferRegion(out_commandBuffer[_frameIndex].Get(), CommandBufferCounterOffset2, out_processedCommandBufferCounterReset.Get(), 0, sizeof(UINT));

			computeCommandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(out_commandBuffer[_frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			computeCommandList->Dispatch(1, 1, 1);
		}

		_commandContext.executeCommandList(computeCommandListSet);
		_commandContext.discardCommandListSet(computeCommandListSet);

		_commandContext.waitForIdle();
	}


	auto commandListSet = _commandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	//デスクリプタヒープをセット
	RefPtr<ID3D12DescriptorHeap> ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	commandList->SetDescriptorHeaps(1, ppHeap);

	//ビューポート設定
	commandList->RSSetViewports(1, &_viewPort);
	commandList->RSSetScissorRects(1, &_scissorRect);

	//コマンド積む用のリソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//レンダーターゲット・デプスステンシルバッファをセット
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _currentFrameResource->_rtv.cpuHandle;
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &_dsv.cpuHandle);

	//レンダーターゲットクリア
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(_dsv.cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//マテリアルの定数バッファをGPUへアップロード
	RefPtr<Camera> mainCamera = _gpuResourceManager.getMainCamera();
	auto& materials = _gpuResourceManager.getMaterials();
	for (auto& material : materials) {
		SharedMaterial& sharedMaterial = material.second;
		sharedMaterial.setParameter<Matrix4>("mtxView", mainCamera->getViewMatrixTransposed());
		sharedMaterial.setParameter<Matrix4>("mtxProj", mainCamera->getProjectionMatrixTransposed());
		sharedMaterial.setParameter<Vector3>("cameraPos", mainCamera->getPosition());
		sharedMaterial._vertexConstantBuffer.flashBufferData(_frameIndex);
		sharedMaterial._pixelConstantBuffer.flashBufferData(_frameIndex);
	}

	//メッシュを描画
	RenderSettings renderSettings(commandList, _frameIndex);

	union GpuCommandGroup {
		RefPtr<StaticSingleMeshRCG> rcg;
		byte* bytePtr;
	};

	GpuCommandGroup group;
	//リニアアロケーターに連続して描画コマンドが存在するので
	//ポインタを任意でオフセットしながらレンダーコマンドグループのデータを参照してループする
	group.rcg = reinterpret_cast<StaticSingleMeshRCG*>(_gpuCommandArray.mainMemory);
	for (uint32 i = 0; i < _gpuCommandCount; ++i) {
		group.rcg->setupRenderCommand(renderSettings);
		group.bytePtr += group.rcg->getRequireMemorySize();
	}

	//デバッグ描画コマンド発効　1フレームごとに描画リストはクリーンアップされる
	_debugGeometryRender.updatePerInstanceData(_frameIndex);
	_debugGeometryRender.setupRenderCommand(renderSettings);
	_debugGeometryRender.clearDebugDatas();

	mat->setupRenderCommand(renderSettings);

	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(out_processedCommandBuffer[_frameIndex].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(out_commandBuffer[_frameIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

	//D3D12_VERTEX_BUFFER_VIEW uavVertexBufferView;
	//uavVertexBufferView.BufferLocation = out_processedCommandBuffer[_frameIndex]->GetGPUVirtualAddress();
	//uavVertexBufferView.StrideInBytes = sizeof(PerInstanceIndirect);
	//uavVertexBufferView.SizeInBytes = CommandBufferCounterOffset + sizeof(UINT);

	//D3D12_VERTEX_BUFFER_VIEW views[2] = { viBuffer->vertexBuffer._vertexBufferView ,uavVertexBufferView };
	//commandList->IASetVertexBuffers(0, 2, views);
	commandList->IASetVertexBuffers(0, 1, &viBuffer->vertexBuffer._vertexBufferView);
	commandList->IASetIndexBuffer(&viBuffer->indexBuffer._indexBufferView);

	//EXECUTION WARNING #1044: GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE
	//IndirectAtgumentバッファに含まれるバーテックスバッファを一度UAVとして扱うのでGPUデバッグレイヤー上でリソースの追跡ができないと警告
	commandList->ExecuteIndirect(
		m_commandSignature.Get(),
		1,
		out_commandBuffer[_frameIndex].Get(),
		0,
		out_commandBuffer[_frameIndex].Get(),
		CommandBufferCounterOffset2);
	//commandList->DrawIndexedInstanced(viBuffer->indexBuffer._indexCount, 128, 0, 0, 0);

	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(out_processedCommandBuffer[_frameIndex].Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(out_commandBuffer[_frameIndex].Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST));

	//ImguiWindow描画
	_imguiWindow.renderFrame(commandList);

	//描画用リソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//コマンドキューにコマンドリストを渡して実行
	_commandContext.executeCommandList(commandListSet);
	_commandContext.discardCommandListSet(commandListSet);

	//画面表示
	throwIfFailed(_swapChain->Present(1, 0));

	//描画完了まで待機
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//描画中に破棄してしまうと困るので待つ
	_commandContext.waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].shutdown();
	}

	gpuCullingCameraInfo.shutdown();

	_gpuCommandArray.shutdown();
	_debugGeometryRender.destroy();

	_imguiWindow.shutdown();
	_gpuResourceManager.shutdown();

	_commandContext.shutdown();
	_descriptorHeapManager.shutdown();

	_swapChain = nullptr;
	_device = nullptr;
}

void GraphicsCore::createTextures(const VectorArray<String>& textureNames) {
	_gpuResourceManager.createTextures(_device.Get(), _commandContext, textureNames);
}

void GraphicsCore::createMeshSets(const VectorArray<String>& fileNames) {
	_gpuResourceManager.createVertexAndIndexBuffer(_device.Get(), _commandContext, fileNames);
}

void GraphicsCore::createSharedMaterial(const SharedMaterialCreateSettings& settings) {
	_gpuResourceManager.createSharedMaterial(_device.Get(), settings);
}

StaticSingleMeshRender GraphicsCore::createStaticSingleMeshRender(const String& name, const VectorArray<String>& materialNames) {
	//メッシュインスタンス生成
	RefPtr<VertexAndIndexBuffer> buffers;
	_gpuResourceManager.loadVertexAndIndexBuffer(name, &buffers);

	const size_t materialCount = buffers->materialDrawRanges.size();
	VectorArray<MaterialSlot> slots;
	VectorArray<RefPtr<SharedMaterial>> materials;
	materials.reserve(materialCount);
	slots.reserve(materialCount);

	//マテリアルスロットにマテリアルをセット
	for (size_t i = 0; i < materialNames.size(); ++i) {
		RefPtr<SharedMaterial> material;
		_gpuResourceManager.loadSharedMaterial(materialNames[i], &material);

		slots.emplace_back(buffers->materialDrawRanges[i], material->getRefSharedMaterial());
		materials.emplace_back(material);
	}

	_gpuCommandCount++;

	const size_t memorySize = StaticSingleMeshRCG::getRequireMemorySize(slots.size());
	byte* r = _gpuCommandArray.divideMemory(memorySize);

	//アロケートしたメモリの先頭からRCGインスタンスを生成
	RefPtr<StaticSingleMeshRCG> render = new (r) StaticSingleMeshRCG(
		buffers->vertexBuffer.getRefVertexBufferView(),
		buffers->indexBuffer.getRefIndexBufferView(),
		slots);

	return StaticSingleMeshRender(materials, render);
}

RefPtr<GpuResourceManager> GraphicsCore::getGpuResourceManager() {
	return &_gpuResourceManager;
}

RefPtr<DebugGeometryRender> GraphicsCore::getDebugGeometryRender() {
	return &_debugGeometryRender;
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
