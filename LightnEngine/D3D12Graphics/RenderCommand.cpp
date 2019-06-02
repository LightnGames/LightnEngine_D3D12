#include "RenderCommand.h"
#include "DebugGeometry.h"

void StaticSingleMesh::create(RefPtr<ID3D12Device> device, const String& meshName, const ConstantBufferFrame& cameraBuffer, const VectorArray<InitSettingsPerSingleMesh>& materialInfos) {
	GpuResourceManager& manager = GpuResourceManager::instance();
	manager.loadVertexAndIndexBuffer(meshName, &_mesh);

	_materials.resize(_mesh->materialDrawRanges.size());
	for (size_t i = 0; i < _materials.size(); ++i) {
		const InitSettingsPerSingleMesh& initInfo = materialInfos[i];
		MaterialCommandGraphics& material = _materials[i];

		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		VertexShader vs;
		PixelShader ps;
		vs.create(initInfo.vertexShaderName, inputLayouts);
		ps.create(initInfo.pixelShaderName);

		uint32 textureCount = initInfo.textureNames.size();

		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.BaseShaderRegister = 0;
		srvRange.NumDescriptors = textureCount;
		srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		VectorArray<D3D12_ROOT_PARAMETER1> parameterDescs(3);
		parameterDescs[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDescs[0].Descriptor.ShaderRegister = 0;
		parameterDescs[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDescs[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[1].DescriptorTable.NumDescriptorRanges = 1;
		parameterDescs[1].DescriptorTable.pDescriptorRanges = &srvRange;

		parameterDescs[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		parameterDescs[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDescs[2].Constants.Num32BitValues = 16;
		parameterDescs[2].Constants.ShaderRegister = 1;

		DescriptorHeapManager& descriptorManager = DescriptorHeapManager::instance();
		GpuResourceManager& resourceManager = GpuResourceManager::instance();

		D3D12_STATIC_SAMPLER_DESC samplerDesc = WrapSamplerDesc();
		DefaultPipelineStateDescSet pipelineStateDesc;

		String shaderNameSet = initInfo.getShaderNameSet();
		RefPtr<RootSignature> rootSignature;
		RefPtr<PipelineState> pipelineState;
		rootSignature = resourceManager.createRootSignature(device, shaderNameSet, parameterDescs, &samplerDesc);
		pipelineState = resourceManager.createPipelineState(device, shaderNameSet, rootSignature, &vs, &ps, pipelineStateDesc);

		material._rootSignature = rootSignature->getRefRootSignature();
		material._pipelineState = pipelineState->getRefPipelineState();

		material._descriptorPerFrames.resize(1);
		material._descriptorPerFrames[0].rootParameterIndex = 0;
		material._descriptorPerFrames[0].resourceAddress[0] = cameraBuffer.constantBuffers[0][0]->getGpuVirtualAddress();
		material._descriptorPerFrames[0].resourceAddress[1] = cameraBuffer.constantBuffers[1][0]->getGpuVirtualAddress();
		material._descriptorPerFrames[0].resourceAddress[2] = cameraBuffer.constantBuffers[2][0]->getGpuVirtualAddress();
		material._descriptorPerFrames[0].type = ResourceType::CONSTANT_BUFFER;

		material._descriptors.resize(1);
		material._descriptors[0].rootParameterIndex = 1;
		material._descriptors[0].viewAddress = resourceManager.createTextureBufferView(shaderNameSet, initInfo.textureNames)->getRefBufferView(0);

		material._rootConstants.resize(1);
		material._rootConstants[0].rootParameterIndex = 2;
		material._rootConstants[0].dataPtr.resize(sizeof(Matrix4));

		material._topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

void StaticSingleMesh::setupRenderCommand(RenderSettings& settings) {
	for (size_t i = 0; i < _mesh->materialDrawRanges.size(); ++i) {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		_materials[i].setupCommand(settings);

		commandList->IASetVertexBuffers(0, 1, &_mesh->vertexBuffer._vertexBufferView);
		commandList->IASetIndexBuffer(&_mesh->indexBuffer._indexBufferView);
		commandList->DrawIndexedInstanced(_mesh->materialDrawRanges[i].indexCount, 1, _mesh->materialDrawRanges[i].indexOffset, 0, 0);
	}
}

void StaticSingleMesh::updateWorldMatrix(const Matrix4& worldMatrix) {
	for (auto&& material : _materials) {
		memcpy(material._rootConstants[0].dataPtr.data(), &worldMatrix, sizeof(worldMatrix));
	}
}

void StaticMultiMesh::create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const ConstantBufferFrame& cameraBuffer, const InitSettingsPerStaticMultiMesh& initInfo) {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	GpuResourceManager& gpuResourceManager = GpuResourceManager::instance();

	//const String& materialName = initInfo.materialName;
	const auto& meshes = initInfo.meshes;
	const auto& textureNames = initInfo.textureNames;
	const uint32 textureCount = static_cast<uint32>(textureNames.size());

	////Shader Resource View作成
	//VectorArray<RefPtr<ID3D12Resource>> ppTextureResources(textureNames.size());
	//for (size_t i = 0; i < textureNames.size(); ++i) {
	//	RefPtr<Texture2D> texture;
	//	gpuResourceManager.loadTexture(textureNames[i], &texture);
	//	ppTextureResources[i] = texture->get();
	//}

	//descriptorHeapManager.createTextureShaderResourceView(ppTextureResources.data(), &srv, textureCount);

	RefPtr<BufferView> v = gpuResourceManager.createTextureBufferView("ggg", textureNames);
	_drawCommand._descriptors.emplace_back(1, v->getRefBufferView(0));

	_drawCommand._descriptorPerFrames.resize(1);
	_drawCommand._descriptorPerFrames[0].rootParameterIndex = 0;
	_drawCommand._descriptorPerFrames[0].resourceAddress[0] = cameraBuffer.constantBuffers[0][0]->getGpuVirtualAddress();
	_drawCommand._descriptorPerFrames[0].resourceAddress[1] = cameraBuffer.constantBuffers[1][0]->getGpuVirtualAddress();
	_drawCommand._descriptorPerFrames[0].resourceAddress[2] = cameraBuffer.constantBuffers[2][0]->getGpuVirtualAddress();
	_drawCommand._descriptorPerFrames[0].type = ResourceType::CONSTANT_BUFFER;

	_drawCommand._rootConstants.resize(1);
	_drawCommand._rootConstants[0].rootParameterIndex = 2;
	_drawCommand._rootConstants[0].dataPtr.resize(sizeof(Vector4));

	_drawCommand._topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	{
		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};

		VertexShader vs;
		PixelShader ps;
		vs.create("Shaders/Indirect.hlsl", inputLayouts);
		ps.create("Shaders/Indirect.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);

		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.BaseShaderRegister = 0;
		srvRange.NumDescriptors = textureCount;
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		VectorArray<D3D12_ROOT_PARAMETER1> parameterDescs(3);
		parameterDescs[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDescs[0].Descriptor.ShaderRegister = 0;
		parameterDescs[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDescs[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[1].DescriptorTable.NumDescriptorRanges = 1;
		parameterDescs[1].DescriptorTable.pDescriptorRanges = &srvRange;

		parameterDescs[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		parameterDescs[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[2].Constants.Num32BitValues = 4;
		parameterDescs[2].Constants.ShaderRegister = 0;

		DefaultPipelineStateDescSet psoDescSet;
		D3D12_STATIC_SAMPLER_DESC samplerDesc = WrapSamplerDesc();

		RefPtr<RootSignature> rootSignature = gpuResourceManager.createRootSignature(device, "ggg", parameterDescs, &samplerDesc);
		RefPtr<PipelineState> pipelineState = gpuResourceManager.createPipelineState(device, "ggg", rootSignature, &vs, &ps, psoDescSet);
		//rootSignature.create(device, parameterDescs, &samplerDesc);
		//pipelineState.create(device, &rootSignature, &vs, &ps);

		_drawCommand._rootSignature = rootSignature->getRefRootSignature();
		_drawCommand._pipelineState = pipelineState->getRefPipelineState();

		//描画元情報からGPUカリングとIndirect描画に必要な情報をまとめる
		_meshCount = static_cast<uint32>(meshes.size());
		_uavCounterOffsets.resize(_meshCount);

		for (uint32 i = 0; i < _meshCount; ++i) {
			_indirectArgumentCount += static_cast<uint32>(meshes[i].textureIndices.size());
			_uavCounterOffsets[i] = AlignForUavCounter(static_cast<uint32>(meshes[i].matrices.size() * sizeof(InstacingVertexData)));
		}
	}

	//コマンドシグネチャ生成
	{

		_indirectArgumentDstCounterOffset = AlignForUavCounter(_indirectArgumentCount * sizeof(IndirectCommand));

		// Each command consists of a CBV update and a DrawInstanced call.
		VectorArray<D3D12_INDIRECT_ARGUMENT_DESC> argumentDescs(5);
		argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
		argumentDescs[0].VertexBuffer.Slot = 0;
		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
		argumentDescs[1].VertexBuffer.Slot = 0;
		argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
		argumentDescs[2].VertexBuffer.Slot = 1;
		argumentDescs[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
		argumentDescs[3].Constant.Num32BitValuesToSet = 4;
		argumentDescs[3].Constant.DestOffsetIn32BitValues = 0;
		argumentDescs[3].Constant.RootParameterIndex = 2;
		argumentDescs[4].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		_commandSignature.create(device, sizeof(IndirectCommand), argumentDescs, _drawCommand._rootSignature.rootSignature);
	}

	//GPUカリングステート
	D3D12_HEAP_PROPERTIES defaultHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };
	D3D12_HEAP_PROPERTIES uploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
	{
		_gpuCullingCameraConstantBuffers[0] = gpuResourceManager.createConstantBuffer(device, "cb1", sizeof(GpuCullingCameraConstant));
		_gpuCullingCameraConstantBuffers[1] = gpuResourceManager.createConstantBuffer(device, "cb2", sizeof(GpuCullingCameraConstant));
		_gpuCullingCameraConstantBuffers[2] = gpuResourceManager.createConstantBuffer(device, "cb3", sizeof(GpuCullingCameraConstant));
		DescriptorPerFrameSet frameSet;
		frameSet.rootParameterIndex = 2;
		frameSet.resourceAddress[0] = _gpuCullingCameraConstantBuffers[0]->getGpuVirtualAddress();
		frameSet.resourceAddress[1] = _gpuCullingCameraConstantBuffers[1]->getGpuVirtualAddress();
		frameSet.resourceAddress[2] = _gpuCullingCameraConstantBuffers[2]->getGpuVirtualAddress();
		frameSet.type = ResourceType::CONSTANT_BUFFER;
		_gpuCullingCommand._descriptorPerFrames.emplace_back(frameSet);
		//_gpuCullingCameraConstantBuffers.create(device, { sizeof(GpuCullingCameraConstant) });

		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 1;
		srvRange.BaseShaderRegister = 0;
		srvRange.RegisterSpace = 0;
		srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 uavRange = {};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = _meshCount;
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

		RefPtr<RootSignature> rootSignature = gpuResourceManager.createRootSignature(device, "ccvvvv", parameterDesc);
		_gpuCullingCommand._rootSignature = rootSignature->getRefRootSignature();
		//_cullingComputeRootSignature.create(device, parameterDesc);

		ComputeShader computeShader;
		computeShader.create("Shaders/GpuCulling_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);//DynamicIndexingを使用

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = rootSignature->_rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();

		RefPtr<PipelineState> pipelineState = gpuResourceManager.createComputePipelineState(device, "compcomp", computePsoDesc);
		_gpuCullingCommand._pipelineState = pipelineState->getRefPipelineState();
		//_cullingComputeState.createCompute(device, computePsoDesc);
	}

	//IndirectCommandステート
	{
		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = _meshCount;
		srvRange.BaseShaderRegister = 2;
		srvRange.RegisterSpace = 0;
		//srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
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
		//parameterDesc[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDesc[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameterDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[1].Descriptor.ShaderRegister = 1;
		//parameterDesc[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDesc[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[2].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[2].DescriptorTable.pDescriptorRanges = &srvRange;

		parameterDesc[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameterDesc[3].DescriptorTable.NumDescriptorRanges = 1;
		parameterDesc[3].DescriptorTable.pDescriptorRanges = &uavRange;

		RefPtr<RootSignature> rootSignature = gpuResourceManager.createRootSignature(device, "ccvvvv2", parameterDesc);
		_setupIndirectArgumentCommand._rootSignature = rootSignature->getRefRootSignature();
		//_setupCommandComputeRootSignature.create(device, parameterDesc);

		ComputeShader computeShader;
		computeShader.create("Shaders/SetupIndirectCommand_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);//DynamicIndexingを使用

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = rootSignature->_rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();

		RefPtr<PipelineState> pipelineState = gpuResourceManager.createComputePipelineState(device, "compcomp2", computePsoDesc);
		_setupIndirectArgumentCommand._pipelineState = pipelineState->getRefPipelineState();
		//_setupCommandComputeState.createCompute(device, computePsoDesc);
	}

	ComPtr<ID3D12Resource> inCommandUploadBuffers;
	ComPtr<ID3D12Resource> indirectCommandUploadBuffers;

	auto commandListSet = commandContext->requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	//カリング前のシーンに配置されているオブジェクトの行列バッファ
	uint32 totalMaxInstanceCount = 0;
	for (const auto& indirectMesh : meshes) {
		totalMaxInstanceCount += static_cast<uint32>(indirectMesh.matrices.size());
	}

	VectorArray<PerInstanceMeshInfo> mergedMatrices;
	mergedMatrices.reserve(totalMaxInstanceCount);

#ifdef ENABLE_AABB_DEBUG_DRAW
	_boundingBoxies.reserve(totalMaxInstanceCount);
#endif

	for (uint32 i = 0; i < _meshCount; ++i) {
		for (uint32 j = 0; j < meshes[i].matrices.size(); ++j) {
			//バウンディングボックス情報が必要なので取得
			RefPtr<VertexAndIndexBuffer> meshVertexAndIndex;
			gpuResourceManager.loadVertexAndIndexBuffer(initInfo.meshNames[i], &meshVertexAndIndex);

			//AABB情報を集める
			const Matrix4& mtxWorld = meshes[i].matrices[j];
			AABB boundingBox = meshVertexAndIndex->boundingBox.createTransformMatrix(mtxWorld);
			boundingBox.translate(mtxWorld.translate());

#ifdef ENABLE_AABB_DEBUG_DRAW
			_boundingBoxies.emplace_back(boundingBox);//デバッグ用
#endif

			PerInstanceMeshInfo info;
			info.indirectArgumentIndex = i;
			info.mtxWorld = mtxWorld.transpose();
			info.boundingBox = boundingBox;

			mergedMatrices.emplace_back(info);
		}
	}

	//トータルのインスタンス数を256の数(Thread X Num)でアラインして何回Dispatchするか決める
	constexpr uint32 THREAD_BLOCK_SIZE = 256;
	_gpuCullingDispatchCount = ((mergedMatrices.size() + (THREAD_BLOCK_SIZE - 1)) & ~(THREAD_BLOCK_SIZE - 1)) / THREAD_BLOCK_SIZE;

	RefPtr<GpuBuffer> gpuDrivenInstanceMatrixBuffer = gpuResourceManager.createOnlyGpuBuffer("vvvvtt1");
	gpuDrivenInstanceMatrixBuffer->createDeferredGpuOnly<PerInstanceMeshInfo>(device, commandList, &inCommandUploadBuffers, mergedMatrices);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(gpuDrivenInstanceMatrixBuffer->get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	//_gpuDrivenInstanceMatrixBuffer.createDeferredGpuOnly<PerInstanceMeshInfo>(device, commandList, &inCommandUploadBuffers, mergedMatrices);
	//commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_gpuDrivenInstanceMatrixBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	D3D12_BUFFER_SRV matrixSrvDesc = {};
	matrixSrvDesc.FirstElement = 0;
	matrixSrvDesc.NumElements = static_cast<UINT>(mergedMatrices.size());
	matrixSrvDesc.StructureByteStride = sizeof(PerInstanceMeshInfo);
	matrixSrvDesc.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	RefPtr<BufferView> gpuDrivenInstanceMatrixView = gpuResourceManager.createOnlyBufferView("trtrtrtr");
	descriptorHeapManager.createShaderResourceView(gpuDrivenInstanceMatrixBuffer->getAdressOf(), gpuDrivenInstanceMatrixView, 1, { matrixSrvDesc });
	//descriptorHeapManager.createShaderResourceView(_gpuDrivenInstanceMatrixBuffer.getAdressOf(), &_gpuDrivenInstanceMatrixView, 1, { matrixSrvDesc });
	_gpuCullingCommand._descriptors.emplace_back(0, gpuDrivenInstanceMatrixView->getRefBufferView(0));


	//インスタンス用行列頂点バッファとそのUAVを生成
	VectorArray<D3D12_BUFFER_UAV> gpuDrivenInstanceCulledBufferUavs(_meshCount);
	VectorArray<D3D12_BUFFER_SRV> gpuDrivenInstanceCulledBufferSrvs(_meshCount);

	for (size_t i = 0; i < gpuDrivenInstanceCulledBufferUavs.size(); ++i) {
		D3D12_BUFFER_UAV& bufferUav = gpuDrivenInstanceCulledBufferUavs[i];
		bufferUav.FirstElement = 0;
		bufferUav.NumElements = static_cast<uint32>(meshes[i].matrices.size());
		bufferUav.StructureByteStride = sizeof(InstacingVertexData);
		bufferUav.CounterOffsetInBytes = _uavCounterOffsets[i];

		//ByteAddressBufferとして読み込んでAppendStructuredBufferのカウンタの値を読み取る
		constexpr UINT CountLength = 1;
		D3D12_BUFFER_SRV& bufferSrv = gpuDrivenInstanceCulledBufferSrvs[i];
		bufferSrv.FirstElement = 0;
		bufferSrv.NumElements = static_cast<UINT>(bufferUav.CounterOffsetInBytes / sizeof(UINT) + CountLength);//4byteオフセットの数で指定する
		bufferSrv.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	}

	//GPUカリング後のバッファをバインドするためのSRVとUAVを作成
	VectorArray<RefPtr<ID3D12Resource>> ppCulledBuffers(_meshCount);
	_gpuDrivenInstanceCulledBuffer.resize(_meshCount);

	for (uint32 i = 0; i < _meshCount; ++i) {
		uint32 index = i;
		char str = i;
		_gpuDrivenInstanceCulledBuffer[index] = gpuResourceManager.createOnlyGpuBuffer(String("gpgpgp_").append(String(&str)));
		_gpuDrivenInstanceCulledBuffer[index]->createDirectGpuOnlyEmpty(device, _uavCounterOffsets[i] + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ppCulledBuffers[i] = _gpuDrivenInstanceCulledBuffer[index]->get();
	}

	//descriptorHeapManager.createUnorederdAcsessView(ppCulledBuffers.data(), &_gpuDriventInstanceCulledUAV, _meshCount, gpuDrivenInstanceCulledBufferUavs);
	//descriptorHeapManager.createShaderResourceView(ppCulledBuffers.data(), &_gpuDriventInstanceCulledSRV, _meshCount, gpuDrivenInstanceCulledBufferSrvs);

	RefPtr<BufferView> gpuDriventInstanceCulledUAV = gpuResourceManager.createOnlyBufferView("uuuuaaaaavvvvv");
	RefPtr<BufferView> gpuDriventInstanceCulledSRV = gpuResourceManager.createOnlyBufferView("uuuusssssaaaaavvvvv");
	descriptorHeapManager.createUnorederdAcsessView(ppCulledBuffers.data(), gpuDriventInstanceCulledUAV, _meshCount, gpuDrivenInstanceCulledBufferUavs);
	descriptorHeapManager.createShaderResourceView(ppCulledBuffers.data(), gpuDriventInstanceCulledSRV, _meshCount, gpuDrivenInstanceCulledBufferSrvs);
	_gpuCullingCommand._descriptors.emplace_back(1, gpuDriventInstanceCulledUAV->getRefBufferView(0));
	_setupIndirectArgumentCommand._descriptors.emplace_back(2, gpuDriventInstanceCulledSRV->getRefBufferView(0));


	//ExecuteIndirectに渡すIndirectBufferを生成
	VectorArray<InIndirectCommand> commands(_indirectArgumentCount);

	uint32 counter = 0;
	for (uint32 i = 0; i < _meshCount; ++i) {
		const uint32 culledBufferIndex = static_cast<uint32>(i);
		const PerMeshData& meshInfo = meshes[i];
		D3D12_VERTEX_BUFFER_VIEW perInstanceVertexBufferView = {};
		perInstanceVertexBufferView.BufferLocation = _gpuDrivenInstanceCulledBuffer[culledBufferIndex]->getGpuVirtualAddress();
		perInstanceVertexBufferView.StrideInBytes = sizeof(InstacingVertexData);
		perInstanceVertexBufferView.SizeInBytes = _uavCounterOffsets[i] + sizeof(UINT);

		//頂点バッファとインデックスバッファのリソースをロード
		RefPtr<VertexAndIndexBuffer> meshVertexAndIndex;
		gpuResourceManager.loadVertexAndIndexBuffer(initInfo.meshNames[i], &meshVertexAndIndex);

		//メッシュ内のサブメッシュごとにIndirectArgument情報を構築
		for (size_t j = 0; j < meshInfo.textureIndices.size(); ++j) {
			const TextureIndex& textureIndices = meshInfo.textureIndices[j];

			IndirectCommand& command = commands[counter].indirectCommand;
			commands[counter].meshIndex[0] = i;
			command.vertexBufferView = meshVertexAndIndex->vertexBuffer._vertexBufferView;
			command.indexBufferView = meshVertexAndIndex->indexBuffer._indexBufferView;
			command.perInstanceVertexBufferView = perInstanceVertexBufferView;
			command.textureIndices[0] = textureIndices.t1;
			command.textureIndices[1] = textureIndices.t2;
			command.textureIndices[2] = textureIndices.t3;
			command.textureIndices[3] = textureIndices.t4;
			command.drawArguments.IndexCountPerInstance = meshVertexAndIndex->materialDrawRanges[j].indexCount;
			command.drawArguments.StartIndexLocation = meshVertexAndIndex->materialDrawRanges[j].indexOffset;
			command.drawArguments.InstanceCount = 0;
			command.drawArguments.BaseVertexLocation = 0;
			command.drawArguments.StartInstanceLocation = 0;

			counter++;
		}
	}

	RefPtr<GpuBuffer> indirectArgumentSourceBuffer = gpuResourceManager.createOnlyGpuBuffer("iiiiddddddd");
	indirectArgumentSourceBuffer->createDeferredGpuOnly<InIndirectCommand>(device, commandList, &indirectCommandUploadBuffers, commands);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(indirectArgumentSourceBuffer->get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	/*_indirectArgumentSourceBuffer.createDeferredGpuOnly<InIndirectCommand>(device, commandList, &indirectCommandUploadBuffers, commands);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentSourceBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));*/
	GpuResourceSet gSet = { 0, indirectArgumentSourceBuffer->getGpuVirtualAddress(), ResourceType::SHADER_RESOURCE };
	_setupIndirectArgumentCommand._gpuResources.emplace_back(gSet);

	//GPUカリング後のIndirectBuffer
	_indirectArgumentDstBuffer = gpuResourceManager.createOnlyGpuBuffer("iiiidddddddst");
	_indirectArgumentDstBuffer->createDirectGpuOnlyEmpty(device, _indirectArgumentDstCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	//_indirectArgumentDstBuffer.createDirectGpuOnlyEmpty(device, _indirectArgumentDstCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	//IndirectArgumentバッファのUAVを作成
	D3D12_BUFFER_UAV bufferUav;
	bufferUav.FirstElement = 0;
	bufferUav.NumElements = _indirectArgumentCount;
	bufferUav.StructureByteStride = sizeof(IndirectCommand);
	bufferUav.CounterOffsetInBytes = _indirectArgumentDstCounterOffset;

	RefPtr<BufferView> setupCommandUavView = gpuResourceManager.createOnlyBufferView("uuuuaaaaavvvvv2222");
	descriptorHeapManager.createUnorederdAcsessView(_indirectArgumentDstBuffer->getAdressOf(), setupCommandUavView, 1, { bufferUav });
	//descriptorHeapManager.createUnorederdAcsessView(indirectArgumentDstBuffer->getAdressOf(), &_setupCommandUavView, 1, { bufferUav });
	_setupIndirectArgumentCommand._descriptors.emplace_back(3, setupCommandUavView->getRefBufferView(0));

	//CulledBufferのカウンタまでのバイトオフセットを格納するバッファ
	RefPtr<GpuBuffer> indirectArgumentOffsetsBuffer = gpuResourceManager.createOnlyGpuBuffer("ofofofoof");
	ComPtr<ID3D12Resource> offsetsUpload;
	indirectArgumentOffsetsBuffer->createDeferredGpuOnly<uint32>(device, commandList, &offsetsUpload, _uavCounterOffsets);

	GpuResourceSet gSet2 = { 1, indirectArgumentOffsetsBuffer->getGpuVirtualAddress(), ResourceType::SHADER_RESOURCE };
	_setupIndirectArgumentCommand._gpuResources.emplace_back(gSet2);

	//UAVカウンタをリセットするための(UINT)0のバッファを生成
	_uavCounterReset = gpuResourceManager.createOnlyGpuBuffer("ofofofoofcncncnc");
	ComPtr<ID3D12Resource> uavCounterUpload;
	_uavCounterReset->createDeferredGpuOnly<UINT>(device, commandList, &uavCounterUpload, { 0 });

	//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
	commandContext->executeCommandList(commandList);
	commandContext->discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	commandContext->waitForIdle();
}

void StaticMultiMesh::onCompute(RefPtr<CommandContext> commandContext, uint32 frameIndex) {
	auto commandListSet = commandContext->requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;
	RenderSettings renderSettings(commandList, 0, frameIndex);

	//デスクリプタヒープをセット
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	RefPtr<ID3D12DescriptorHeap> ppHeap[] = { descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	commandList->SetDescriptorHeaps(1, ppHeap);

	//AppendStructuredBufferのカウンタを0にリセットする
	for (uint32 i = 0; i < _meshCount; ++i) {
		commandList->CopyBufferRegion(_gpuDrivenInstanceCulledBuffer[i]->get(), _uavCounterOffsets[i], _uavCounterReset->get(), 0, sizeof(UINT));
	}

	_gpuCullingCommand.setupCommand(renderSettings);
	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, frameIndex);
	commandList->Dispatch(_gpuCullingDispatchCount, 1, 1);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, frameIndex);
	_setupIndirectArgumentCommand.setupCommand(renderSettings);

	//AppendStructuredBufferのカウンタを0にリセットする
	commandList->CopyBufferRegion(_indirectArgumentDstBuffer->get(), _indirectArgumentDstCounterOffset, _uavCounterReset->get(), 0, sizeof(UINT));
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer->get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	commandList->Dispatch(_indirectArgumentCount, 1, 1);

	commandContext->executeCommandList(commandListSet);
	commandContext->discardCommandListSet(commandListSet);

	commandContext->waitForIdle();
}

#include "ThirdParty/Imgui/imgui.h"
void StaticMultiMesh::setupCommand(RenderSettings & settings) {
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	uint32 frameIndex = settings.frameIndex;

	static Vector3 positionV = -Vector3::forward * 23 + Vector3::up * 5;
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

	Camera virtualCamera;
	virtualCamera.setPosition(positionV);
	virtualCamera.setRotationEuler(pitchV, yawV, rollV, true);
	virtualCamera.setFieldOfView(fovV);
	virtualCamera.setNearZ(nearZV);
	virtualCamera.setFarZ(farZV);
	virtualCamera.setAspectRate(1280, 720);
	virtualCamera.computeProjectionMatrix();
	virtualCamera.computeViewMatrix();

	virtualCamera.computeFlustomNormals();
	virtualCamera.debugDrawFlustom();

	updateCullingCameraInfo(virtualCamera, frameIndex);

	_drawCommand.setupCommand(settings);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, frameIndex);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer->get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

	//EXECUTION WARNING #1044: GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE
	//IndirectAtgumentバッファに含まれるバーテックスバッファを一度UAVとして扱うのでGPUデバッグレイヤー上でリソースの追跡ができないと警告
	commandList->ExecuteIndirect(
		_commandSignature._commandSignature.Get(),
		_indirectArgumentCount,
		_indirectArgumentDstBuffer->get(),
		0,
		_indirectArgumentDstBuffer->get(),
		_indirectArgumentDstCounterOffset);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST, frameIndex);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer->get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST));

	DebugGeometryRender& debug = DebugGeometryRender::instance();
	for (const auto& aabb : _boundingBoxies) {
		debug.debugDrawCube(aabb.center(), Quaternion::identity, aabb.size());
	}
}

void StaticMultiMesh::updateCullingCameraInfo(const Camera & camera, uint32 frameIndex) {
	GpuCullingCameraConstant gpuCullingConstant;
	gpuCullingConstant.cameraPosition = camera.getPosition();

	for (uint32 i = 0; i < 4; ++i) {
		gpuCullingConstant.frustumPlanes[i] = camera.getFrustumPlaneNormal(i);
	}

	_gpuCullingCameraConstantBuffers[frameIndex]->writeData(&gpuCullingConstant, sizeof(gpuCullingConstant));
}

void StaticMultiMesh::culledBufferBarrier(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, uint32 frameIndex) const {
	VectorArray<D3D12_RESOURCE_BARRIER> barriers(_meshCount);
	for (uint32 i = 0; i < _meshCount; ++i) {
		barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[i].Transition.StateBefore = StateBefore;
		barriers[i].Transition.StateAfter = StateAfter;
		barriers[i].Transition.pResource = _gpuDrivenInstanceCulledBuffer[i]->get();
	}

	commandList->ResourceBarrier(_meshCount, barriers.data());
}
