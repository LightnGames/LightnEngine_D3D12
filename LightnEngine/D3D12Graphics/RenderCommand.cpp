#include "RenderCommand.h"
#include "DebugGeometry.h"
#include "DescriptorHeap.h"
#include "GpuResourceManager.h"

void StaticSingleMesh::create(RefPtr<ID3D12Device> device, const String& meshName, const ConstantBufferFrame& cameraBuffer, const VectorArray<InitSettingsPerSingleMesh>& materialInfos) {
	DescriptorHeapManager& descriptorManager = DescriptorHeapManager::instance();
	GpuResourceManager& resourceManager = GpuResourceManager::instance();

	resourceManager.loadVertexAndIndexBuffer(meshName, &_mesh);
	_mainMaterials.resize(_mesh->materialDrawRanges.size());
	_depthMaterials.resize(_mainMaterials.size());

	for (size_t i = 0; i < _mainMaterials.size(); ++i) {
		const InitSettingsPerSingleMesh& initInfo = materialInfos[i];
		const String shaderNameSet = initInfo.getShaderNameSet();
		MaterialCommandGraphics& material = _mainMaterials[i];

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

		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.BaseShaderRegister = 0;
		srvRange.NumDescriptors = static_cast<uint32>(initInfo.textureNames.size());
		srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		VectorArray<D3D12_ROOT_PARAMETER1> mainParameterDescs(3);
		mainParameterDescs[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		mainParameterDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		mainParameterDescs[0].Descriptor.ShaderRegister = 0;
		mainParameterDescs[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		mainParameterDescs[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		mainParameterDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		mainParameterDescs[1].Constants.Num32BitValues = 16;
		mainParameterDescs[1].Constants.ShaderRegister = 1;

		mainParameterDescs[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		mainParameterDescs[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		mainParameterDescs[2].DescriptorTable.NumDescriptorRanges = 1;
		mainParameterDescs[2].DescriptorTable.pDescriptorRanges = &srvRange;

		D3D12_STATIC_SAMPLER_DESC samplerDesc = WrapSamplerDesc();

		//MainPass用パイプラインステート＆ルートシグネチャ
		{
			RefPtr<RootSignature> rootSignature;
			RefPtr<PipelineState> pipelineState;

			DefaultPipelineStateDescSet pipelineStateDesc;
			pipelineStateDesc.dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;

			rootSignature = resourceManager.createRootSignature(device, shaderNameSet, mainParameterDescs, &samplerDesc);
			pipelineState = resourceManager.createPipelineState(device, shaderNameSet, rootSignature, &vs, &ps, pipelineStateDesc);

			material._rootSignature = rootSignature->getRefRootSignature();
			material._pipelineState = pipelineState->getRefPipelineState(); 
		}

		GpuResourcePerFrameSet cameraSet;
		cameraSet.rootParameterIndex = 0;
		cameraSet.resourceAddress[0] = cameraBuffer.constantBuffers[0].getGpuVirtualAddress();
		cameraSet.resourceAddress[1] = cameraBuffer.constantBuffers[1].getGpuVirtualAddress();
		cameraSet.resourceAddress[2] = cameraBuffer.constantBuffers[2].getGpuVirtualAddress();
		cameraSet.type = ResourceType::CONSTANT_BUFFER;
		material._gpuResourcePerFrames.emplace_back(cameraSet);

		material._descriptors.emplace_back(2, resourceManager.createTextureBufferView(shaderNameSet, initInfo.textureNames)->getRefBufferView());

		RootConstantSet rootConstantSet;
		rootConstantSet.rootParameterIndex = 1;
		rootConstantSet.dataPtr.resize(sizeof(Matrix4));

		material._rootConstants.emplace_back(rootConstantSet);
		material._topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		//以下DepthPrePass用マテリアル
		const String depthShaderNameSet = shaderNameSet + String("_DepthPass");
		MaterialCommandGraphics& depthMaterial = _depthMaterials[i];
		VectorArray<D3D12_ROOT_PARAMETER1> depthParameterDescs(2);
		depthParameterDescs[0] = mainParameterDescs[0];
		depthParameterDescs[1] = mainParameterDescs[1];

		//Depth用パイプラインステート＆ルートシグネチャ
		{
			RefPtr<RootSignature> rootSignature;
			RefPtr<PipelineState> pipelineState;
			DefaultPipelineStateDescSet pipelineStateDesc;
			rootSignature = resourceManager.createRootSignature(device, depthShaderNameSet, depthParameterDescs, nullptr);
			pipelineState = resourceManager.createPipelineState(device, depthShaderNameSet, rootSignature, &vs, nullptr, pipelineStateDesc);

			depthMaterial._rootSignature = rootSignature->getRefRootSignature();
			depthMaterial._pipelineState = pipelineState->getRefPipelineState();
		}

		depthMaterial._gpuResourcePerFrames.emplace_back(cameraSet);
		depthMaterial._rootConstants.emplace_back(rootConstantSet);
		depthMaterial._topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

void StaticSingleMesh::setupDepthPassCommand(RenderSettings& settings){
	for (size_t i = 0; i < _mesh->materialDrawRanges.size(); ++i) {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		_depthMaterials[i].setupCommand(settings);

		commandList->IASetVertexBuffers(0, 1, &_mesh->vertexBuffer._vertexBufferView);
		commandList->IASetIndexBuffer(&_mesh->indexBuffer._indexBufferView);
		commandList->DrawIndexedInstanced(_mesh->materialDrawRanges[i].indexCount, 1, _mesh->materialDrawRanges[i].indexOffset, 0, 0);
	}
}

void StaticSingleMesh::setupMainPassCommand(RenderSettings& settings) {
	for (size_t i = 0; i < _mesh->materialDrawRanges.size(); ++i) {
		RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
		_mainMaterials[i].setupCommand(settings);

		commandList->IASetVertexBuffers(0, 1, &_mesh->vertexBuffer._vertexBufferView);
		commandList->IASetIndexBuffer(&_mesh->indexBuffer._indexBufferView);
		commandList->DrawIndexedInstanced(_mesh->materialDrawRanges[i].indexCount, 1, _mesh->materialDrawRanges[i].indexOffset, 0, 0);
	}
}

void StaticSingleMesh::updateWorldMatrix(const Matrix4& worldMatrix) {
	for (size_t i = 0; i < _mesh->materialDrawRanges.size(); ++i) {
		memcpy(_mainMaterials[i]._rootConstants[0].dataPtr.data(), &worldMatrix, sizeof(worldMatrix));
		memcpy(_depthMaterials[i]._rootConstants[0].dataPtr.data(), &worldMatrix, sizeof(worldMatrix));
	}
}

void StaticMultiMesh::create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const InitBufferInfo& bufferInfo, const InitSettingsPerStaticMultiMesh& initInfo) {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	GpuResourceManager& gpuResourceManager = GpuResourceManager::instance();

	const String materialName("GpuDrivenMaterial");
	const auto& meshes = initInfo.meshes;
	const auto& textureNames = initInfo.textureNames;
	const uint32 textureCount = static_cast<uint32>(textureNames.size());

	RefPtr<BufferView> textureSRV = gpuResourceManager.createTextureBufferView(materialName + String("_Textures"), textureNames);
	_mainPassCommand._descriptors.emplace_back(3, textureSRV->getRefBufferView());

	String diffuseEnv("cubemapEnvHDR.dds");
	String specularEnv("cubemapSpecularHDR.dds");
	String specularBrdf("cubemapBrdf.dds");
	RefPtr<BufferView> environmentSRV = gpuResourceManager.createTextureBufferView(materialName + String("_EnvTextures"), { diffuseEnv,specularEnv,specularBrdf });
	_mainPassCommand._descriptors.emplace_back(2, environmentSRV->getRefBufferView());

	GpuResourcePerFrameSet cameraSet;
	bufferInfo.cameraBuffer.getVirtualAdresses(cameraSet.resourceAddress);
	cameraSet.rootParameterIndex = 0;
	cameraSet.type = ResourceType::CONSTANT_BUFFER;
	_mainPassCommand._gpuResourcePerFrames.emplace_back(cameraSet);
	_depthPassCommand._gpuResourcePerFrames.emplace_back(cameraSet);

	GpuResourcePerFrameSet directionalLightSet;
	bufferInfo.directionalLightBuffer.getVirtualAdresses(directionalLightSet.resourceAddress);
	directionalLightSet.rootParameterIndex = 4;
	directionalLightSet.type = ResourceType::CONSTANT_BUFFER;
	_mainPassCommand._gpuResourcePerFrames.emplace_back(directionalLightSet);

	GpuResourcePerFrameSet pointLightSet;
	bufferInfo.pointLightBuffer.getVirtualAdresses(pointLightSet.resourceAddress);
	pointLightSet.rootParameterIndex = 5;
	pointLightSet.type = ResourceType::CONSTANT_BUFFER;
	_mainPassCommand._gpuResourcePerFrames.emplace_back(pointLightSet);

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

		D3D12_DESCRIPTOR_RANGE1 environmentSrvRange = {};
		environmentSrvRange.BaseShaderRegister = 0;
		environmentSrvRange.NumDescriptors = 3;
		environmentSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		environmentSrvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		environmentSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE1 textureSrvRange = {};
		textureSrvRange.BaseShaderRegister = 3;
		textureSrvRange.NumDescriptors = textureCount;
		textureSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		textureSrvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		textureSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		VectorArray<D3D12_ROOT_PARAMETER1> parameterDescs(6);
		parameterDescs[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDescs[0].Descriptor.ShaderRegister = 0;
		parameterDescs[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDescs[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		parameterDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameterDescs[1].Constants.Num32BitValues = 4;
		parameterDescs[1].Constants.ShaderRegister = 1;

		parameterDescs[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDescs[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[2].DescriptorTable.NumDescriptorRanges = 1;
		parameterDescs[2].DescriptorTable.pDescriptorRanges = &environmentSrvRange;

		parameterDescs[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDescs[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[3].DescriptorTable.NumDescriptorRanges = 1;
		parameterDescs[3].DescriptorTable.pDescriptorRanges = &textureSrvRange;

		parameterDescs[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDescs[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[4].Descriptor.ShaderRegister = 0;
		parameterDescs[4].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		parameterDescs[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameterDescs[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameterDescs[5].Descriptor.ShaderRegister = 1;
		parameterDescs[5].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

		D3D12_STATIC_SAMPLER_DESC samplerDesc = WrapSamplerDesc();

		//デプスパス
		{
			VectorArray<D3D12_ROOT_PARAMETER1> depthParameterDescs(2);
			depthParameterDescs[0] = parameterDescs[0];
			depthParameterDescs[1] = parameterDescs[1];

			DefaultPipelineStateDescSet psoDescSet;
			RefPtr<RootSignature> rootSignature = gpuResourceManager.createRootSignature(device, materialName + "_DepthPass", depthParameterDescs, nullptr);
			RefPtr<PipelineState> pipelineState = gpuResourceManager.createPipelineState(device, materialName + "_DepthPass", rootSignature, &vs, nullptr, psoDescSet);

			_depthPassCommand._rootSignature = rootSignature->getRefRootSignature();
			_depthPassCommand._pipelineState = pipelineState->getRefPipelineState();
			_depthPassCommand._topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}

		//メインパス
		{
			DefaultPipelineStateDescSet psoDescSet;
			psoDescSet.dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
			RefPtr<RootSignature> rootSignature = gpuResourceManager.createRootSignature(device, materialName + "_MainPass", parameterDescs, &samplerDesc);
			RefPtr<PipelineState> pipelineState = gpuResourceManager.createPipelineState(device, materialName + "_MainPass", rootSignature, &vs, &ps, psoDescSet);

			_mainPassCommand._rootSignature = rootSignature->getRefRootSignature();
			_mainPassCommand._pipelineState = pipelineState->getRefPipelineState(); 
			_mainPassCommand._topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}

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
		argumentDescs[3].Constant.RootParameterIndex = 1;
		argumentDescs[3].Constant.Num32BitValuesToSet = 4;
		argumentDescs[3].Constant.DestOffsetIn32BitValues = 0;
		argumentDescs[4].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		_depthPassCommandSignature.create(device, sizeof(IndirectCommand), argumentDescs, _depthPassCommand._rootSignature.rootSignature);
		_mainPassCommandSignature.create(device, sizeof(IndirectCommand), argumentDescs, _mainPassCommand._rootSignature.rootSignature);
	}

	//GPUカリングステート
	D3D12_HEAP_PROPERTIES defaultHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };
	D3D12_HEAP_PROPERTIES uploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
	{
		_gpuCullingCameraConstantBuffers[0] = gpuResourceManager.createConstantBuffer(device, materialName + "_cb1", sizeof(GpuCullingCameraConstant));
		_gpuCullingCameraConstantBuffers[1] = gpuResourceManager.createConstantBuffer(device, materialName + "_cb2", sizeof(GpuCullingCameraConstant));
		_gpuCullingCameraConstantBuffers[2] = gpuResourceManager.createConstantBuffer(device, materialName + "_cb3", sizeof(GpuCullingCameraConstant));
		
		GpuResourcePerFrameSet frameSet;
		frameSet.rootParameterIndex = 2;
		frameSet.resourceAddress[0] = _gpuCullingCameraConstantBuffers[0]->getGpuVirtualAddress();
		frameSet.resourceAddress[1] = _gpuCullingCameraConstantBuffers[1]->getGpuVirtualAddress();
		frameSet.resourceAddress[2] = _gpuCullingCameraConstantBuffers[2]->getGpuVirtualAddress();
		frameSet.type = ResourceType::CONSTANT_BUFFER;
		_gpuCullingCommand._gpuResourcePerFrames.emplace_back(frameSet);

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

		RefPtr<RootSignature> rootSignature = gpuResourceManager.createRootSignature(device, materialName + "_GpuCullingCommand", parameterDesc);
		_gpuCullingCommand._rootSignature = rootSignature->getRefRootSignature();

		ComputeShader computeShader;
		computeShader.create("Shaders/GpuCulling_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);//DynamicIndexingを使用

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = rootSignature->_rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();

		RefPtr<PipelineState> pipelineState = gpuResourceManager.createComputePipelineState(device, materialName + "_GpuCullingCommand", computePsoDesc);
		_gpuCullingCommand._pipelineState = pipelineState->getRefPipelineState();
	}

	//IndirectCommandステート
	{
		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = _meshCount;
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

		RefPtr<RootSignature> rootSignature = gpuResourceManager.createRootSignature(device, materialName + "_IndirectArgumentCommand", parameterDesc);
		_setupIndirectArgumentCommand._rootSignature = rootSignature->getRefRootSignature();

		ComputeShader computeShader;
		computeShader.create("Shaders/SetupIndirectCommand_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);//DynamicIndexingを使用

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = rootSignature->_rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();

		RefPtr<PipelineState> pipelineState = gpuResourceManager.createComputePipelineState(device, materialName + "_IndirectArgumentCommand", computePsoDesc);
		_setupIndirectArgumentCommand._pipelineState = pipelineState->getRefPipelineState();
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

	RefPtr<GpuBuffer> gpuDrivenInstanceMatrixBuffer = gpuResourceManager.createOnlyGpuBuffer(materialName + "_GpuDrivenInstanceMatrix");
	gpuDrivenInstanceMatrixBuffer->createDeferredGpuOnly<PerInstanceMeshInfo>(device, commandList, &inCommandUploadBuffers, mergedMatrices);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(gpuDrivenInstanceMatrixBuffer->get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	D3D12_BUFFER_SRV matrixSrvDesc = {};
	matrixSrvDesc.FirstElement = 0;
	matrixSrvDesc.NumElements = static_cast<UINT>(mergedMatrices.size());
	matrixSrvDesc.StructureByteStride = sizeof(PerInstanceMeshInfo);
	matrixSrvDesc.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	RefPtr<BufferView> gpuDrivenInstanceMatrixSRV = gpuResourceManager.createOnlyBufferView(materialName + "_GpuDrivenInstanceMatrix");
	descriptorHeapManager.createShaderResourceView(gpuDrivenInstanceMatrixBuffer->getAdressOf(), gpuDrivenInstanceMatrixSRV, 1, { matrixSrvDesc });
	_gpuCullingCommand._descriptors.emplace_back(0, gpuDrivenInstanceMatrixSRV->getRefBufferView());


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
	_gpuDrivenInstanceCulledBuffers.resize(_meshCount);

	for (uint32 i = 0; i < _meshCount; ++i) {
		char str = i;
		_gpuDrivenInstanceCulledBuffers[i] = gpuResourceManager.createOnlyGpuBuffer(materialName + "_GpuDrivenInstanceCulled_" + String(&str));
		_gpuDrivenInstanceCulledBuffers[i]->createDirectGpuOnlyEmpty(device, _uavCounterOffsets[i] + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ppCulledBuffers[i] = _gpuDrivenInstanceCulledBuffers[i]->get();
	}

	RefPtr<BufferView> gpuDriventInstanceCulledUAV = gpuResourceManager.createOnlyBufferView(materialName + "_GpuDrivenInstanceCulled_UAV");
	RefPtr<BufferView> gpuDriventInstanceCulledSRV = gpuResourceManager.createOnlyBufferView(materialName + "_GpuDrivenInstanceCulled_SRV");
	descriptorHeapManager.createUnorederdAcsessView(ppCulledBuffers.data(), gpuDriventInstanceCulledUAV, _meshCount, gpuDrivenInstanceCulledBufferUavs);
	descriptorHeapManager.createShaderResourceView(ppCulledBuffers.data(), gpuDriventInstanceCulledSRV, _meshCount, gpuDrivenInstanceCulledBufferSrvs);
	_gpuCullingCommand._descriptors.emplace_back(1, gpuDriventInstanceCulledUAV->getRefBufferView());
	_setupIndirectArgumentCommand._descriptors.emplace_back(2, gpuDriventInstanceCulledSRV->getRefBufferView());

	//ExecuteIndirectに渡すIndirectBufferを生成
	VectorArray<InIndirectCommand> commands(_indirectArgumentCount);

	uint32 counter = 0;
	for (uint32 i = 0; i < _meshCount; ++i) {
		const uint32 culledBufferIndex = static_cast<uint32>(i);
		const PerMeshData& meshInfo = meshes[i];
		D3D12_VERTEX_BUFFER_VIEW perInstanceVertexBufferView = {};
		perInstanceVertexBufferView.BufferLocation = _gpuDrivenInstanceCulledBuffers[culledBufferIndex]->getGpuVirtualAddress();
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
			command.textureIndices = textureIndices;
			command.drawArguments.IndexCountPerInstance = meshVertexAndIndex->materialDrawRanges[j].indexCount;
			command.drawArguments.StartIndexLocation = meshVertexAndIndex->materialDrawRanges[j].indexOffset;
			command.drawArguments.InstanceCount = 0;
			command.drawArguments.BaseVertexLocation = 0;
			command.drawArguments.StartInstanceLocation = 0;

			counter++;
		}
	}

	RefPtr<GpuBuffer> indirectArgumentSourceBuffer = gpuResourceManager.createOnlyGpuBuffer(materialName + "_IndirectArgumentSource");
	indirectArgumentSourceBuffer->createDeferredGpuOnly<InIndirectCommand>(device, commandList, &indirectCommandUploadBuffers, commands);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(indirectArgumentSourceBuffer->get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	GpuResourceSet gSet = { 0, indirectArgumentSourceBuffer->getGpuVirtualAddress(), ResourceType::SHADER_RESOURCE };
	_setupIndirectArgumentCommand._gpuResources.emplace_back(gSet);

	//GPUカリング後のIndirectBuffer
	_indirectArgumentDstBuffer = gpuResourceManager.createOnlyGpuBuffer(materialName + "_IndirectArgumentDst");
	_indirectArgumentDstBuffer->createDirectGpuOnlyEmpty(device, _indirectArgumentDstCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	//IndirectArgumentバッファのUAVを作成
	D3D12_BUFFER_UAV bufferUav;
	bufferUav.FirstElement = 0;
	bufferUav.NumElements = _indirectArgumentCount;
	bufferUav.StructureByteStride = sizeof(IndirectCommand);
	bufferUav.CounterOffsetInBytes = _indirectArgumentDstCounterOffset;

	RefPtr<BufferView> setupCommandUAV = gpuResourceManager.createOnlyBufferView(materialName + "_SetupCommand_UAV");
	descriptorHeapManager.createUnorederdAcsessView(_indirectArgumentDstBuffer->getAdressOf(), setupCommandUAV, 1, { bufferUav });
	_setupIndirectArgumentCommand._descriptors.emplace_back(3, setupCommandUAV->getRefBufferView());

	//CulledBufferのカウンタまでのバイトオフセットを格納するバッファ
	RefPtr<GpuBuffer> indirectArgumentOffsetsBuffer = gpuResourceManager.createOnlyGpuBuffer(materialName + "_IndirectArgumentOffsets");
	ComPtr<ID3D12Resource> offsetsUpload;
	indirectArgumentOffsetsBuffer->createDeferredGpuOnly<uint32>(device, commandList, &offsetsUpload, _uavCounterOffsets);

	GpuResourceSet gSet2 = { 1, indirectArgumentOffsetsBuffer->getGpuVirtualAddress(), ResourceType::SHADER_RESOURCE };
	_setupIndirectArgumentCommand._gpuResources.emplace_back(gSet2);

	//UAVカウンタをリセットするための(UINT)0のバッファを生成
	ComPtr<ID3D12Resource> uavCounterUpload;
	_uavCounterReset = gpuResourceManager.createOnlyGpuBuffer(materialName + "_UavCounterReset");
	_uavCounterReset->createDeferredGpuOnly<UINT>(device, commandList, &uavCounterUpload, { 0 });

	//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
	commandContext->executeCommandList(commandList);
	commandContext->discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	commandContext->waitForIdle();
}

void StaticMultiMesh::onCompute(RenderSettings & settings) {
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;

	//AppendStructuredBufferのカウンタを0にリセットする
	for (uint32 i = 0; i < _meshCount; ++i) {
		commandList->CopyBufferRegion(_gpuDrivenInstanceCulledBuffers[i]->get(), _uavCounterOffsets[i], _uavCounterReset->get(), 0, sizeof(UINT));
	}

	_gpuCullingCommand.setupCommand(settings);
	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, frameIndex);
	commandList->Dispatch(_gpuCullingDispatchCount, 1, 1);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, frameIndex);
	_setupIndirectArgumentCommand.setupCommand(settings);

	//AppendStructuredBufferのカウンタを0にリセットする
	commandList->CopyBufferRegion(_indirectArgumentDstBuffer->get(), _indirectArgumentDstCounterOffset, _uavCounterReset->get(), 0, sizeof(UINT));
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer->get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	commandList->Dispatch(_indirectArgumentCount, 1, 1);
}

#include "ThirdParty/Imgui/imgui.h"
void StaticMultiMesh::setupDepthPassCommand(RenderSettings& settings){
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	uint32 frameIndex = settings.frameIndex;

	static Vector3 positionV = -Vector3::forward * 40 + Vector3::up * 5;
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

	_depthPassCommand.setupCommand(settings);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, frameIndex);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer->get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

	//EXECUTION WARNING #1044: GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE
	//IndirectAtgumentバッファに含まれるバーテックスバッファを一度UAVとして扱うのでGPUデバッグレイヤー上でリソースの追跡ができないと警告
	commandList->ExecuteIndirect(
		_depthPassCommandSignature._commandSignature.Get(),
		_indirectArgumentCount,
		_indirectArgumentDstBuffer->get(),
		0,
		_indirectArgumentDstBuffer->get(),
		_indirectArgumentDstCounterOffset);
}

void StaticMultiMesh::setupMainPassCommand(RenderSettings & settings) {
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	uint32 frameIndex = settings.frameIndex;

	_mainPassCommand.setupCommand(settings);

	//EXECUTION WARNING #1044: GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE
	//IndirectAtgumentバッファに含まれるバーテックスバッファを一度UAVとして扱うのでGPUデバッグレイヤー上でリソースの追跡ができないと警告
	commandList->ExecuteIndirect(
		_mainPassCommandSignature._commandSignature.Get(),
		_indirectArgumentCount,
		_indirectArgumentDstBuffer->get(),
		0,
		_indirectArgumentDstBuffer->get(),
		_indirectArgumentDstCounterOffset);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST, frameIndex);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer->get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST));

	//Debug用バウンディングボックスAABBを描画
#ifdef ENABLE_AABB_DEBUG_DRAW
	DebugGeometryRender& debug = DebugGeometryRender::instance();
	for (const auto& aabb : _boundingBoxies) {
		debug.debugDrawCube(aabb.center(), Quaternion::identity, aabb.size());
	}
#endif
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
		barriers[i].Transition.pResource = _gpuDrivenInstanceCulledBuffers[i]->get();
	}

	commandList->ResourceBarrier(_meshCount, barriers.data());
}
