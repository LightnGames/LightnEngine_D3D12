#include "GraphicsCore.h"
#include <stdexcept>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "SharedMaterial.h"
#include "MeshRender.h"

#include "ThirdParty/Imgui/imgui.h"

struct IndirectCommand
{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	D3D12_VERTEX_BUFFER_VIEW perInstanceVertexBufferView;
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
};

struct ObjectInfo {
	Matrix4 mtxWorld;
	Vector3 startPosAABB;
	Vector3 endposAABB;
	Color color;
};

struct PerInstanceVertex {
	Matrix4 mtxWorld;
	Color color;
};

struct PerInstanceIndirectArgument {
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	uint32 indexCount;
	uint32 counterOffset;
	uint32 instanceCount;
};

UINT IndirectArgumentCount;
UINT IndirectArgumentDstCounterOffset;

RefPtr<SharedMaterial> mat;
RefPtr<VertexAndIndexBuffer> viBuffer;
RefPtr<VertexAndIndexBuffer> viBuffer2;

VectorArray<PerInstanceIndirectArgument> indirectMeshes;

CommandSignature _commandSignature;
RootSignature _cullingComputeRootSignature;
PipelineState _cullingComputeState;
RootSignature _setupCommandComputeRootSignature;
PipelineState _setupCommandComputeState;

GpuBuffer* _gpuDrivenInstanceMatrixBuffer;//カリング前のシーンに配置されているインスタンスのワールド行列
GpuBuffer* _gpuDrivenInstanceCulledBuffer;//GPUカリング後の描画対象のインスタンスのワールド行列
GpuBuffer _indirectArgumentDstBuffer[FrameCount];//GPUカリング後のExecuteIndirectに渡される描画引数
GpuBuffer _indirectArgumentSourceBuffer[FrameCount];//カリング前のシーンに配置されているインスタンスの描画引数
GpuBuffer _indirectArgumentOffsetsBuffer;
GpuBuffer _uavCounterReset;//UAVのAppendStructuredBufferのカウント引数を０に戻すためのUINT１つのバッファ

BufferView setupCommandUavView[FrameCount];
BufferView _gpuDriventInstanceCulledSRV[FrameCount];//GPUカリング後の情報を読み込むバッファのSRV
BufferView _gpuDriventInstanceCulledUAV[FrameCount];//GPUカリング後の情報を書き込むバッファのUAV
BufferView _gpuDrivenInstanceMatrixView;

ConstantBufferMaterial gpuCullingCameraInfo;

bool gpuDrivenStenby = false;

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

	//コマンドコンテキスト生成
	_graphicsCommandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
	_computeCommandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);

	//Imgui初期化
	_imguiWindow.init(hwnd, _device.Get());

	//デバッグ描画機能初期化
	_debugGeometryRender.create(_device.Get(), &_graphicsCommandContext);

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
		throwIfFailed(factory->CreateSwapChainForHwnd(_graphicsCommandContext.getDirectQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));


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

	IndirectArgumentCount = 2;
	indirectMeshes.resize(IndirectArgumentCount);

	_gpuResourceManager.createVertexAndIndexBuffer(_device.Get(), _graphicsCommandContext, { "cube.mesh", "sphere.mesh" });
	_gpuResourceManager.loadVertexAndIndexBuffer("cube.mesh", &viBuffer);
	_gpuResourceManager.loadVertexAndIndexBuffer("sphere.mesh", &viBuffer2);

	indirectMeshes[0].vertexBufferView = viBuffer->vertexBuffer._vertexBufferView;
	indirectMeshes[0].indexBufferView = viBuffer->indexBuffer._indexBufferView;
	indirectMeshes[0].indexCount = viBuffer->indexBuffer._indexCount;
	indirectMeshes[0].instanceCount = 128;
	indirectMeshes[0].counterOffset = AlignForUavCounter(indirectMeshes[0].instanceCount * sizeof(PerInstanceVertex));

	indirectMeshes[1].vertexBufferView = viBuffer2->vertexBuffer._vertexBufferView;
	indirectMeshes[1].indexBufferView = viBuffer2->indexBuffer._indexBufferView;
	indirectMeshes[1].indexCount = viBuffer2->indexBuffer._indexCount;
	indirectMeshes[1].instanceCount = 128;
	indirectMeshes[1].counterOffset = AlignForUavCounter(indirectMeshes[1].instanceCount * sizeof(PerInstanceVertex));

	VectorArray<VectorArray<ObjectInfo>> objectInfos(IndirectArgumentCount);

	for (size_t i = 0; i < objectInfos.size(); ++i) {
		const uint32 instanceCount = indirectMeshes[i].instanceCount;
		VectorArray<ObjectInfo>& objectArray = objectInfos[i];
		objectArray.resize(instanceCount);

		//シーンに配置されているオブジェクトのワールド行列をマップ
		for (size_t j = 0; j < objectArray.size(); ++j) {
			Vector3 position(j * 1.2f - instanceCount / 2, (float)i, 2);
			objectArray[j].mtxWorld = Matrix4::translateXYZ(position).transpose();
			objectArray[j].startPosAABB = position;
			objectArray[j].color = Color(j * 0.01f, 0, 0, 1);
		}
	}

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

		_gpuDrivenInstanceCulledBuffer = new GpuBuffer[IndirectArgumentCount * FrameCount];
		_gpuDrivenInstanceMatrixBuffer = new GpuBuffer[IndirectArgumentCount];

		IndirectArgumentDstCounterOffset = AlignForUavCounter(IndirectArgumentCount * sizeof(IndirectCommand));

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
		VectorArray<D3D12_INDIRECT_ARGUMENT_DESC> argumentDescs(4);
		argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
		argumentDescs[0].VertexBuffer.Slot = 0;
		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
		argumentDescs[1].VertexBuffer.Slot = 0;
		argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
		argumentDescs[2].VertexBuffer.Slot = 1;
		argumentDescs[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		_commandSignature.create(_device.Get(), sizeof(IndirectCommand), argumentDescs);
	}

	//GPUカリングステート
	D3D12_HEAP_PROPERTIES defaultHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };
	D3D12_HEAP_PROPERTIES uploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
	{
		gpuCullingCameraInfo.create(_device.Get(), { sizeof(SceneConstant) });

		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = IndirectArgumentCount;
		srvRange.BaseShaderRegister = 0;
		srvRange.RegisterSpace = 0;
		srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 uavRange = {};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = IndirectArgumentCount;
		uavRange.BaseShaderRegister = 0;
		uavRange.RegisterSpace = 0;
		uavRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		VectorArray<D3D12_ROOT_PARAMETER1> parameterDesc(3);
		parameterDesc[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[0].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[0].DescriptorTable.pDescriptorRanges = &srvRange;

		parameterDesc[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[1].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[1].DescriptorTable.pDescriptorRanges = &uavRange;

		parameterDesc[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDesc[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[2].Descriptor.ShaderRegister = 0;
		parameterDesc[2].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		_cullingComputeRootSignature.create(_device.Get(), parameterDesc);

		ComputeShader computeShader;
		computeShader.create("Shaders/GpuCulling_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);

		// Describe and create the compute pipeline state object (PSO).
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = _cullingComputeRootSignature._rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();
		_cullingComputeState.createCompute(_device.Get(), computePsoDesc);
	}

	//IndirectCommandステート
	{
		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = IndirectArgumentCount;
		srvRange.BaseShaderRegister = 2;
		srvRange.RegisterSpace = 0;
		srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 uavRange = {};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = 1;
		uavRange.BaseShaderRegister = 0;
		uavRange.RegisterSpace = 0;
		uavRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		VectorArray<D3D12_ROOT_PARAMETER1> parameterDesc(4);
		parameterDesc[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameterDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[0].Descriptor.ShaderRegister = 0;
		parameterDesc[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDesc[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameterDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[1].Descriptor.ShaderRegister = 1;
		parameterDesc[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDesc[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[2].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[2].DescriptorTable.pDescriptorRanges = &srvRange;

		parameterDesc[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[3].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[3].DescriptorTable.pDescriptorRanges = &uavRange;

		_setupCommandComputeRootSignature.create(_device.Get(), parameterDesc);

		ComputeShader computeShader;
		computeShader.create("Shaders/SetupIndirectCommand_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);

		// Describe and create the compute pipeline state object (PSO).
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = _setupCommandComputeRootSignature._rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();
		_setupCommandComputeState.createCompute(_device.Get(), computePsoDesc);
	}

	VectorArray<ComPtr<ID3D12Resource>> inCommandUploadBuffers(IndirectArgumentCount);
	ComPtr<ID3D12Resource> indirectCommandUploadBuffers[FrameCount];

	auto commandListSet = _graphicsCommandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	//カリング前のシーンに配置されているオブジェクトの行列バッファ
	VectorArray<RefPtr<ID3D12Resource>> ppMatrixBuffers(IndirectArgumentCount);
	for (uint32 i = 0; i < IndirectArgumentCount; ++i) {
		_gpuDrivenInstanceMatrixBuffer[i].createDeferredGpuOnly<ObjectInfo>(_device.Get(), commandList, &inCommandUploadBuffers[i], objectInfos[i]);
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_gpuDrivenInstanceMatrixBuffer[i].get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		ppMatrixBuffers[i] = _gpuDrivenInstanceMatrixBuffer[i].get();
	}

	VectorArray<D3D12_BUFFER_SRV> matrixSrvDesc(IndirectArgumentCount);
	for (size_t i = 0; i < matrixSrvDesc.size(); ++i) {
		D3D12_BUFFER_SRV& srvDesc = matrixSrvDesc[i];
		srvDesc.FirstElement = 0;
		srvDesc.NumElements = indirectMeshes[i].instanceCount;
		srvDesc.StructureByteStride = sizeof(ObjectInfo);
		srvDesc.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	}

	_descriptorHeapManager.createShaderResourceView(ppMatrixBuffers.data(), &_gpuDrivenInstanceMatrixView, IndirectArgumentCount, matrixSrvDesc);

	//インスタンス用行列頂点バッファとそのUAVを生成
	VectorArray<D3D12_BUFFER_UAV> gpuDrivenInstanceCulledBufferUavs(IndirectArgumentCount);
	VectorArray<D3D12_BUFFER_SRV> gpuDrivenInstanceCulledBufferSrvs(IndirectArgumentCount);

	for (size_t i = 0; i < gpuDrivenInstanceCulledBufferUavs.size(); ++i) {
		D3D12_BUFFER_UAV& bufferUav = gpuDrivenInstanceCulledBufferUavs[i];
		bufferUav.FirstElement = 0;
		bufferUav.NumElements = indirectMeshes[i].instanceCount;
		bufferUav.StructureByteStride = sizeof(PerInstanceVertex);
		bufferUav.CounterOffsetInBytes = indirectMeshes[i].counterOffset;

		//ByteAddressBufferとして読み込んでAppendStructuredBufferのカウンタの値を読み取る
		D3D12_BUFFER_SRV& bufferSrv = gpuDrivenInstanceCulledBufferSrvs[i];
		bufferSrv.FirstElement = 0;
		bufferSrv.NumElements = bufferUav.CounterOffsetInBytes / sizeof(UINT) + 1;//4byteオフセットの数で指定する
		bufferSrv.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	}

	for (uint32 i = 0; i < FrameCount; ++i) {
		VectorArray<RefPtr<ID3D12Resource>> ppCulledBuffers(IndirectArgumentCount);

		for (uint32 j = 0; j < IndirectArgumentCount; ++j) {
			uint32 index = i * IndirectArgumentCount + j;
			_gpuDrivenInstanceCulledBuffer[index].createDirectGpuOnlyEmpty(_device.Get(), indirectMeshes[j].counterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			ppCulledBuffers[j] = _gpuDrivenInstanceCulledBuffer[index].get();
		}

		_descriptorHeapManager.createUnorederdAcsessView(ppCulledBuffers.data(), &_gpuDriventInstanceCulledUAV[i], IndirectArgumentCount, gpuDrivenInstanceCulledBufferUavs);
		_descriptorHeapManager.createShaderResourceView(ppCulledBuffers.data(), &_gpuDriventInstanceCulledSRV[i], IndirectArgumentCount, gpuDrivenInstanceCulledBufferSrvs);
	}

	//ExecuteIndirectに渡すIndirectBufferを生成
	for (uint32 i = 0; i < FrameCount; ++i) {
		VectorArray<IndirectCommand> commands(IndirectArgumentCount);

		for (size_t j = 0; j < indirectMeshes.size(); ++j) {
			const uint32 culledBufferIndex = i * IndirectArgumentCount + j;
			PerInstanceIndirectArgument& mesh = indirectMeshes[j];

			D3D12_VERTEX_BUFFER_VIEW perInstanceVertexBufferView = {};
			perInstanceVertexBufferView.BufferLocation = _gpuDrivenInstanceCulledBuffer[culledBufferIndex].getGpuVirtualAddress();
			perInstanceVertexBufferView.StrideInBytes = sizeof(PerInstanceVertex);
			perInstanceVertexBufferView.SizeInBytes = mesh.counterOffset + sizeof(UINT);

			commands[j].vertexBufferView = mesh.vertexBufferView;
			commands[j].indexBufferView = mesh.indexBufferView;
			commands[j].perInstanceVertexBufferView = perInstanceVertexBufferView;
			commands[j].drawArguments.IndexCountPerInstance = mesh.indexCount;
			commands[j].drawArguments.InstanceCount = 0;
			commands[j].drawArguments.BaseVertexLocation = 0;
			commands[j].drawArguments.StartIndexLocation = 0;
			commands[j].drawArguments.StartInstanceLocation = 0;
		}

		_indirectArgumentSourceBuffer[i].createDeferredGpuOnly<IndirectCommand>(_device.Get(), commandList, &indirectCommandUploadBuffers[i], commands);
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentSourceBuffer[i].get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		//GPUカリング後のIndirectBuffer
		_indirectArgumentDstBuffer[i].createDirectGpuOnlyEmpty(_device.Get(), IndirectArgumentDstCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		//IndirectArgumentバッファのUAVを作成
		D3D12_BUFFER_UAV bufferUav;
		bufferUav.FirstElement = 0;
		bufferUav.NumElements = IndirectArgumentCount;
		bufferUav.StructureByteStride = sizeof(IndirectCommand);
		bufferUav.CounterOffsetInBytes = IndirectArgumentDstCounterOffset;

		_descriptorHeapManager.createUnorederdAcsessView(_indirectArgumentDstBuffer[i].getAdressOf(), &setupCommandUavView[i], 1, { bufferUav });
	}

	ComPtr<ID3D12Resource> offsetsUpload;
	VectorArray<uint32> counterOffsets(IndirectArgumentCount);
	for (size_t i = 0; i < counterOffsets.size(); ++i) {
		counterOffsets[i] = indirectMeshes[i].counterOffset;
	}
	_indirectArgumentOffsetsBuffer.createDeferredGpuOnly<uint32>(_device.Get(), commandList, &offsetsUpload, counterOffsets);

	//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
	_graphicsCommandContext.executeCommandList(commandList);
	_graphicsCommandContext.discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	_graphicsCommandContext.waitForIdle();

	//UAVカウンタをリセットするための(UINT)0のバッファを生成
	_uavCounterReset.createDirectCpuReadWrite<UINT>(_device.Get(), { 0 });

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

void GraphicsCore::Barrier(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter){
	for (uint32 i = 0; i < IndirectArgumentCount; ++i) {
		uint32 index = _frameIndex * IndirectArgumentCount + i;
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_gpuDrivenInstanceCulledBuffer[index].get(), StateBefore, StateAfter));
	}
}

void GraphicsCore::Reset(RefPtr<ID3D12GraphicsCommandList> commandList){
	for (uint32 i = 0; i < IndirectArgumentCount; ++i) {
		uint32 index = _frameIndex * IndirectArgumentCount + i;
		commandList->CopyBufferRegion(_gpuDrivenInstanceCulledBuffer[index].get(), indirectMeshes[i].counterOffset, _uavCounterReset.get(), 0, sizeof(UINT));
	}
}

void GraphicsCore::onRender() {
	if (gpuDrivenStenby) {
		auto computeCommandListSet = _computeCommandContext.requestCommandListSet();
		RefPtr<ID3D12GraphicsCommandList> computeCommandList = computeCommandListSet.commandList;

		//デスクリプタヒープをセット
		RefPtr<ID3D12DescriptorHeap> ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		computeCommandList->SetDescriptorHeaps(1, ppHeap);

		//GPUカリング
		{
			computeCommandList->SetPipelineState(_cullingComputeState._pipelineState.Get());
			computeCommandList->SetComputeRootSignature(_cullingComputeRootSignature._rootSignature.Get());

			computeCommandList->SetComputeRootDescriptorTable(0, _gpuDrivenInstanceMatrixView.gpuHandle);
			computeCommandList->SetComputeRootDescriptorTable(1, _gpuDriventInstanceCulledUAV[_frameIndex].gpuHandle);
			computeCommandList->SetComputeRootConstantBufferView(2, gpuCullingCameraInfo.constantBuffers[_frameIndex][0]->_resource->GetGPUVirtualAddress());

			//AppendStructuredBufferのカウンタを0にリセットする
			Reset(computeCommandList);
			//computeCommandList->CopyBufferRegion(_gpuDrivenInstanceCulledBuffer[_frameIndex].get(), GpuDrivenInstanceCulledCounterOffset, _uavCounterReset.get(), 0, sizeof(UINT));

			Barrier(computeCommandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			//computeCommandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_gpuDrivenInstanceCulledBuffer[_frameIndex].get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

			computeCommandList->Dispatch(IndirectArgumentCount, 1, 1);
		}

		//ExecuteIndirectコマンド構築
		{
			computeCommandList->SetPipelineState(_setupCommandComputeState._pipelineState.Get());
			computeCommandList->SetComputeRootSignature(_setupCommandComputeRootSignature._rootSignature.Get());

			Barrier(computeCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			//computeCommandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_gpuDrivenInstanceCulledBuffer[_frameIndex].get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

			computeCommandList->SetComputeRootShaderResourceView(0, _indirectArgumentSourceBuffer[_frameIndex].getGpuVirtualAddress());
			computeCommandList->SetComputeRootShaderResourceView(1, _indirectArgumentOffsetsBuffer.getGpuVirtualAddress());
			computeCommandList->SetComputeRootDescriptorTable(2, _gpuDriventInstanceCulledSRV[_frameIndex].gpuHandle);
			computeCommandList->SetComputeRootDescriptorTable(3, setupCommandUavView[_frameIndex].gpuHandle);
			//computeCommandList->SetComputeRootConstantBufferView(3, gpuCullingCameraInfo.constantBuffers[_frameIndex][0]->_resource->GetGPUVirtualAddress());

			//AppendStructuredBufferのカウンタを0にリセットする
			computeCommandList->CopyBufferRegion(_indirectArgumentDstBuffer[_frameIndex].get(), IndirectArgumentDstCounterOffset, _uavCounterReset.get(), 0, sizeof(UINT));

			computeCommandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer[_frameIndex].get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			computeCommandList->Dispatch(IndirectArgumentCount, 1, 1);
		}

		_computeCommandContext.executeCommandList(computeCommandListSet);
		_computeCommandContext.discardCommandListSet(computeCommandListSet);

		_computeCommandContext.waitForIdle();
	}


	auto commandListSet = _graphicsCommandContext.requestCommandListSet();
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

	Barrier(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	//commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_gpuDrivenInstanceCulledBuffer[_frameIndex].get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer[_frameIndex].get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

	//EXECUTION WARNING #1044: GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE
	//IndirectAtgumentバッファに含まれるバーテックスバッファを一度UAVとして扱うのでGPUデバッグレイヤー上でリソースの追跡ができないと警告
	commandList->ExecuteIndirect(
		_commandSignature._commandSignature.Get(),
		IndirectArgumentCount,
		_indirectArgumentDstBuffer[_frameIndex].get(),
		0,
		_indirectArgumentDstBuffer[_frameIndex].get(),
		IndirectArgumentDstCounterOffset);

	Barrier(commandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
	//commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_gpuDrivenInstanceCulledBuffer[_frameIndex].get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer[_frameIndex].get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST));

	//ImguiWindow描画
	_imguiWindow.renderFrame(commandList);

	//描画用リソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//コマンドキューにコマンドリストを渡して実行
	_graphicsCommandContext.executeCommandList(commandListSet);
	_graphicsCommandContext.discardCommandListSet(commandListSet);

	//画面表示
	throwIfFailed(_swapChain->Present(1, 0));

	//描画完了まで待機
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//描画中に破棄してしまうと困るので待つ
	_graphicsCommandContext.waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].shutdown();
		_indirectArgumentSourceBuffer[i]._resource = nullptr;
		_indirectArgumentDstBuffer[i]._resource = nullptr;
	}

	delete[] _gpuDrivenInstanceCulledBuffer;
	delete[] _gpuDrivenInstanceMatrixBuffer;
	_uavCounterReset._resource = nullptr;


	_cullingComputeRootSignature._rootSignature = nullptr;
	_cullingComputeState._pipelineState = nullptr;
	_setupCommandComputeRootSignature._rootSignature = nullptr;
	_setupCommandComputeState._pipelineState = nullptr;
	_commandSignature._commandSignature = nullptr;
	_indirectArgumentOffsetsBuffer.destroy();

	gpuCullingCameraInfo.shutdown();

	_gpuCommandArray.shutdown();
	_debugGeometryRender.destroy();

	_imguiWindow.shutdown();
	_gpuResourceManager.shutdown();

	_graphicsCommandContext.shutdown();
	_computeCommandContext.shutdown();
	_descriptorHeapManager.shutdown();

	_swapChain = nullptr;
	_device = nullptr;
}

void GraphicsCore::createTextures(const VectorArray<String>& textureNames) {
	_gpuResourceManager.createTextures(_device.Get(), _graphicsCommandContext, textureNames);
}

void GraphicsCore::createMeshSets(const VectorArray<String>& fileNames) {
	_gpuResourceManager.createVertexAndIndexBuffer(_device.Get(), _graphicsCommandContext, fileNames);
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
	RefPtr<CommandQueue> commandQueue = _graphicsCommandContext.getCommandQueue();
	const UINT64 currentFenceValue = _frameResources[_frameIndex]._fenceValue;
	commandQueue->waitForFence(currentFenceValue);

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];

	const UINT64 fenceValue = commandQueue->incrementFence();
	_frameResources[_frameIndex]._fenceValue = fenceValue;
}
