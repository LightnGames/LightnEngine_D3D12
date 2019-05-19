#include "StaticMultiMesh.h"
#include "SharedMaterial.h"
#include "GpuResource.h"
#include "DescriptorHeap.h"
#include "GpuResourceManager.h"
#include "MeshRender.h"]
#include "DebugGeometry.h"

void StaticMultiMeshRCG::create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const StaticMultiMeshInitInfo& initInfo) {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	GpuResourceManager& gpuResourceManager = GpuResourceManager::instance();

	const String& materialName = initInfo.materialName;
	const auto& meshes = initInfo.meshes;
	const auto& textureNames = initInfo.textureNames;

	//Shader Resource View�쐬
	VectorArray<RefPtr<ID3D12Resource>> ppTextureResources(textureNames.size());
	for (size_t i = 0; i < textureNames.size(); ++i) {
		RefPtr<Texture2D> texture;
		gpuResourceManager.loadTexture(textureNames[i], &texture);
		ppTextureResources[i] = texture->get();
	}

	uint32 textureCount = static_cast<uint32>(textureNames.size());
	descriptorHeapManager.createTextureShaderResourceView(ppTextureResources.data(), &srv, textureCount);

	cb.create(device, { sizeof(CameraConstantRaw) });

	VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	};

	VertexShader vs;
	PixelShader ps;
	vs.create("Shaders/Indirect.hlsl", inputLayouts);
	ps.create("Shaders/Indirect.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);

	D3D12_DESCRIPTOR_RANGE1 cbvRange = {};
	cbvRange.BaseShaderRegister = 0;
	cbvRange.NumDescriptors = 1;
	cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE1 srvRange = {};
	srvRange.BaseShaderRegister = 0;
	srvRange.NumDescriptors = textureCount;
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	VectorArray<D3D12_ROOT_PARAMETER1> parameterDescs(3);
	parameterDescs[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameterDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	parameterDescs[0].DescriptorTable.NumDescriptorRanges = 1;
	parameterDescs[0].DescriptorTable.pDescriptorRanges = &cbvRange;

	parameterDescs[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameterDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	parameterDescs[1].DescriptorTable.NumDescriptorRanges = 1;
	parameterDescs[1].DescriptorTable.pDescriptorRanges = &srvRange;

	parameterDescs[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameterDescs[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	parameterDescs[2].Constants.Num32BitValues = 4;
	parameterDescs[2].Constants.ShaderRegister = 0;

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 0;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.ShaderRegister = 0;
	samplerDesc.RegisterSpace = 0;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSignature.create(device, parameterDescs, &samplerDesc);
	pipelineState.create(device, &rootSignature, vs, ps);

	//�`�挳��񂩂�GPU�J�����O��Indirect�`��ɕK�v�ȏ����܂Ƃ߂�
	_meshCount = static_cast<uint32>(meshes.size());
	_uavCounterOffsets.resize(_meshCount);

	for (uint32 i = 0; i < _meshCount; ++i) {
		_indirectArgumentCount += static_cast<uint32>(meshes[i].textureIndices.size());
		_uavCounterOffsets[i] = AlignForUavCounter(meshes[i].maxInstanceCount * sizeof(PerInstanceVertex));
	}

	//�R�}���h�V�O�l�`������
	{
		_gpuDrivenInstanceCulledBuffer = new GpuBuffer[_meshCount * FrameCount];
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

		_commandSignature.create(device, sizeof(IndirectCommand), argumentDescs, rootSignature._rootSignature.Get());
	}

	//GPU�J�����O�X�e�[�g
	D3D12_HEAP_PROPERTIES defaultHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };
	D3D12_HEAP_PROPERTIES uploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
	{
		_gpuCullingCameraConstantBuffers.create(device, { sizeof(GpuCullingCameraConstant) });

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

		_cullingComputeRootSignature.create(device, parameterDesc);

		ComputeShader computeShader;
		computeShader.create("Shaders/GpuCulling_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);//DynamicIndexing���g�p

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = _cullingComputeRootSignature._rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();
		_cullingComputeState.createCompute(device, computePsoDesc);
	}

	//IndirectCommand�X�e�[�g
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

		_setupCommandComputeRootSignature.create(device, parameterDesc);

		ComputeShader computeShader;
		computeShader.create("Shaders/SetupIndirectCommand_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);//DynamicIndexing���g�p

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = _setupCommandComputeRootSignature._rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();
		_setupCommandComputeState.createCompute(device, computePsoDesc);
	}

	ComPtr<ID3D12Resource> inCommandUploadBuffers;
	ComPtr<ID3D12Resource> indirectCommandUploadBuffers[FrameCount];

	auto commandListSet = commandContext->requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	//�J�����O�O�̃V�[���ɔz�u����Ă���I�u�W�F�N�g�̍s��o�b�t�@
	uint32 totalMaxInstanceCount = 0;
	for (const auto& indirectMesh : meshes) {
		totalMaxInstanceCount += indirectMesh.maxInstanceCount;
	}

	size_t mergedMatricsOffset = 0;
	VectorArray<ObjectInfo> mergedMatrices(totalMaxInstanceCount);
	for (uint32 i = 0; i < _meshCount; ++i) {
		memcpy(mergedMatrices.data() + mergedMatricsOffset, meshes[i].matrices.data(), meshes[i].matrices.size() * sizeof(ObjectInfo));
		mergedMatricsOffset += meshes[i].matrices.size();
	}

	//�g�[�^���̃C���X�^���X����256�̐�(Thread X Num)�ŃA���C�����ĉ���Dispatch���邩���߂�
	constexpr uint32 THREAD_BLOCK_SIZE = 256;
	_gpuCullingDispatchCount = ((mergedMatrices.size() + (THREAD_BLOCK_SIZE - 1)) & ~(THREAD_BLOCK_SIZE - 1)) / THREAD_BLOCK_SIZE;

	_gpuDrivenInstanceMatrixBuffer.createDeferredGpuOnly<ObjectInfo>(device, commandList, &inCommandUploadBuffers, mergedMatrices);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_gpuDrivenInstanceMatrixBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	D3D12_BUFFER_SRV matrixSrvDesc = {};
	matrixSrvDesc.FirstElement = 0;
	matrixSrvDesc.NumElements = static_cast<UINT>(mergedMatrices.size());
	matrixSrvDesc.StructureByteStride = sizeof(ObjectInfo);
	matrixSrvDesc.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	descriptorHeapManager.createShaderResourceView(_gpuDrivenInstanceMatrixBuffer.getAdressOf(), &_gpuDrivenInstanceMatrixView, 1, { matrixSrvDesc });

	//AABB�����W�߂�
	_boundingBoxies.reserve(totalMaxInstanceCount);
	for (uint32 i = 0; i < _meshCount; ++i) {
		for (uint32 j = 0; j < meshes[i].matrices.size(); ++j) {
			_boundingBoxies.emplace_back(meshes[i].matrices[j].boundingBox);
		}
	}

	//�C���X�^���X�p�s�񒸓_�o�b�t�@�Ƃ���UAV�𐶐�
	VectorArray<D3D12_BUFFER_UAV> gpuDrivenInstanceCulledBufferUavs(_meshCount);
	VectorArray<D3D12_BUFFER_SRV> gpuDrivenInstanceCulledBufferSrvs(_meshCount);

	for (size_t i = 0; i < gpuDrivenInstanceCulledBufferUavs.size(); ++i) {
		D3D12_BUFFER_UAV& bufferUav = gpuDrivenInstanceCulledBufferUavs[i];
		bufferUav.FirstElement = 0;
		bufferUav.NumElements = meshes[i].maxInstanceCount;
		bufferUav.StructureByteStride = sizeof(PerInstanceVertex);
		bufferUav.CounterOffsetInBytes = _uavCounterOffsets[i];

		//ByteAddressBuffer�Ƃ��ēǂݍ����AppendStructuredBuffer�̃J�E���^�̒l��ǂݎ��
		constexpr UINT CountLength = 1;
		D3D12_BUFFER_SRV& bufferSrv = gpuDrivenInstanceCulledBufferSrvs[i];
		bufferSrv.FirstElement = 0;
		bufferSrv.NumElements = static_cast<UINT>(bufferUav.CounterOffsetInBytes / sizeof(UINT) + CountLength);//4byte�I�t�Z�b�g�̐��Ŏw�肷��
		bufferSrv.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	}

	for (uint32 frame = 0; frame < FrameCount; ++frame) {
		VectorArray<RefPtr<ID3D12Resource>> ppCulledBuffers(_meshCount);

		for (uint32 i = 0; i < _meshCount; ++i) {
			uint32 index = frame * _meshCount + i;
			_gpuDrivenInstanceCulledBuffer[index].createDirectGpuOnlyEmpty(device, _uavCounterOffsets[i] + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			ppCulledBuffers[i] = _gpuDrivenInstanceCulledBuffer[index].get();
		}

		descriptorHeapManager.createUnorederdAcsessView(ppCulledBuffers.data(), &_gpuDriventInstanceCulledUAV[frame], _meshCount, gpuDrivenInstanceCulledBufferUavs);
		descriptorHeapManager.createShaderResourceView(ppCulledBuffers.data(), &_gpuDriventInstanceCulledSRV[frame], _meshCount, gpuDrivenInstanceCulledBufferSrvs);
	}

	//ExecuteIndirect�ɓn��IndirectBuffer�𐶐�
	for (uint32 frame = 0; frame < FrameCount; ++frame) {
		VectorArray<InIndirectCommand> commands(_indirectArgumentCount);

		uint32 counter = 0;
		for (uint32 i = 0; i < _meshCount; ++i) {
			const uint32 culledBufferIndex = frame * _meshCount + static_cast<uint32>(i);
			const IndirectMeshInfo& meshInfo = meshes[i];
			D3D12_VERTEX_BUFFER_VIEW perInstanceVertexBufferView = {};
			perInstanceVertexBufferView.BufferLocation = _gpuDrivenInstanceCulledBuffer[culledBufferIndex].getGpuVirtualAddress();
			perInstanceVertexBufferView.StrideInBytes = sizeof(PerInstanceVertex);
			perInstanceVertexBufferView.SizeInBytes = _uavCounterOffsets[i] + sizeof(UINT);

			for (size_t j = 0; j < meshInfo.textureIndices.size(); ++j) {
				const TextureIndex& textureIndices = meshInfo.textureIndices[j];
				const RefPtr<VertexAndIndexBuffer> meshVertexInfo = meshInfo.vertexAndIndexBuffer;

				IndirectCommand& command = commands[counter].indirectCommand;
				commands[counter].meshIndex[0] = i;
				command.vertexBufferView = meshVertexInfo->vertexBuffer._vertexBufferView;
				command.indexBufferView = meshVertexInfo->indexBuffer._indexBufferView;
				command.perInstanceVertexBufferView = perInstanceVertexBufferView;
				command.textureIndices[0] = textureIndices.t1;
				command.textureIndices[1] = textureIndices.t2;
				command.textureIndices[2] = textureIndices.t3;
				command.textureIndices[3] = textureIndices.t4;
				command.drawArguments.IndexCountPerInstance = meshVertexInfo->materialDrawRanges[j].indexCount;
				command.drawArguments.StartIndexLocation = meshVertexInfo->materialDrawRanges[j].indexOffset;
				command.drawArguments.InstanceCount = 0;
				command.drawArguments.BaseVertexLocation = 0;
				command.drawArguments.StartInstanceLocation = 0;

				counter++;
			}
		}

		_indirectArgumentSourceBuffer[frame].createDeferredGpuOnly<InIndirectCommand>(device, commandList, &indirectCommandUploadBuffers[frame], commands);
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentSourceBuffer[frame].get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		//GPU�J�����O���IndirectBuffer
		_indirectArgumentDstBuffer[frame].createDirectGpuOnlyEmpty(device, _indirectArgumentDstCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		//IndirectArgument�o�b�t�@��UAV���쐬
		D3D12_BUFFER_UAV bufferUav;
		bufferUav.FirstElement = 0;
		bufferUav.NumElements = _indirectArgumentCount;
		bufferUav.StructureByteStride = sizeof(IndirectCommand);
		bufferUav.CounterOffsetInBytes = _indirectArgumentDstCounterOffset;

		descriptorHeapManager.createUnorederdAcsessView(_indirectArgumentDstBuffer[frame].getAdressOf(), &_setupCommandUavView[frame], 1, { bufferUav });
	}

	//CulledBuffer�̃J�E���^�܂ł̃o�C�g�I�t�Z�b�g���i�[����o�b�t�@
	ComPtr<ID3D12Resource> offsetsUpload;
	_indirectArgumentOffsetsBuffer.createDeferredGpuOnly<uint32>(device, commandList, &offsetsUpload, _uavCounterOffsets);

	//�R�}���h���s(�A�b�v���[�h�o�b�t�@�̃e�N�X�`������GPU�ǂݏ�������o�b�t�@�ɃR�s�[)
	commandContext->executeCommandList(commandList);
	commandContext->discardCommandListSet(commandListSet);

	//�R�s�[���I���܂ŃA�b�v���[�h�q�[�v��j�����Ȃ�
	commandContext->waitForIdle();

	//UAV�J�E���^�����Z�b�g���邽�߂�(UINT)0�̃o�b�t�@�𐶐�
	_uavCounterReset.createDirectCpuReadWrite<UINT>(device, { 0 });
}

void StaticMultiMeshRCG::onCompute(RefPtr<CommandContext> commandContext, uint32 frameIndex) {
	DescriptorHeapManager& _descriptorHeapManager = DescriptorHeapManager::instance();

	auto computeCommandListSet = commandContext->requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> computeCommandList = computeCommandListSet.commandList;

	//�f�X�N���v�^�q�[�v���Z�b�g
	RefPtr<ID3D12DescriptorHeap> ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	computeCommandList->SetDescriptorHeaps(1, ppHeap);

	//GPU�J�����O
	{
		computeCommandList->SetPipelineState(_cullingComputeState._pipelineState.Get());
		computeCommandList->SetComputeRootSignature(_cullingComputeRootSignature._rootSignature.Get());

		computeCommandList->SetComputeRootDescriptorTable(0, _gpuDrivenInstanceMatrixView.gpuHandle);
		computeCommandList->SetComputeRootDescriptorTable(1, _gpuDriventInstanceCulledUAV[frameIndex].gpuHandle);
		computeCommandList->SetComputeRootConstantBufferView(2, _gpuCullingCameraConstantBuffers.constantBuffers[frameIndex][0]->_resource->GetGPUVirtualAddress());

		//AppendStructuredBuffer�̃J�E���^��0�Ƀ��Z�b�g����
		for (uint32 i = 0; i < _meshCount; ++i) {
			uint32 index = frameIndex * _meshCount + i;
			computeCommandList->CopyBufferRegion(_gpuDrivenInstanceCulledBuffer[index].get(), _uavCounterOffsets[i], _uavCounterReset.get(), 0, sizeof(UINT));
		}

		culledBufferBarrier(computeCommandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, frameIndex);

		computeCommandList->Dispatch(_gpuCullingDispatchCount, 1, 1);
	}

	//ExecuteIndirect�R�}���h�\�z
	{
		computeCommandList->SetPipelineState(_setupCommandComputeState._pipelineState.Get());
		computeCommandList->SetComputeRootSignature(_setupCommandComputeRootSignature._rootSignature.Get());

		culledBufferBarrier(computeCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, frameIndex);

		computeCommandList->SetComputeRootShaderResourceView(0, _indirectArgumentSourceBuffer[frameIndex].getGpuVirtualAddress());
		computeCommandList->SetComputeRootShaderResourceView(1, _indirectArgumentOffsetsBuffer.getGpuVirtualAddress());
		computeCommandList->SetComputeRootDescriptorTable(2, _gpuDriventInstanceCulledSRV[frameIndex].gpuHandle);
		computeCommandList->SetComputeRootDescriptorTable(3, _setupCommandUavView[frameIndex].gpuHandle);

		//AppendStructuredBuffer�̃J�E���^��0�Ƀ��Z�b�g����
		computeCommandList->CopyBufferRegion(_indirectArgumentDstBuffer[frameIndex].get(), _indirectArgumentDstCounterOffset, _uavCounterReset.get(), 0, sizeof(UINT));

		computeCommandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer[frameIndex].get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		computeCommandList->Dispatch(_indirectArgumentCount, 1, 1);
	}


	commandContext->executeCommandList(computeCommandListSet);
	commandContext->discardCommandListSet(computeCommandListSet);

	commandContext->waitForIdle();
}

void StaticMultiMeshRCG::setupRenderCommand(RenderSettings & settings) {
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	uint32 frameIndex = settings.frameIndex;

	//_material->setupRenderCommand(settings);
	commandList->SetGraphicsRootSignature(rootSignature._rootSignature.Get());
	commandList->SetPipelineState(pipelineState._pipelineState.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootDescriptorTable(0, cb.constantBufferViews[frameIndex].gpuHandle);
	commandList->SetGraphicsRootDescriptorTable(1, srv.gpuHandle);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, frameIndex);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer[frameIndex].get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

	//EXECUTION WARNING #1044: GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE
	//IndirectAtgument�o�b�t�@�Ɋ܂܂��o�[�e�b�N�X�o�b�t�@����xUAV�Ƃ��Ĉ����̂�GPU�f�o�b�O���C���[��Ń��\�[�X�̒ǐՂ��ł��Ȃ��ƌx��
	commandList->ExecuteIndirect(
		_commandSignature._commandSignature.Get(),
		_indirectArgumentCount,
		_indirectArgumentDstBuffer[frameIndex].get(),
		0,
		_indirectArgumentDstBuffer[frameIndex].get(),
		_indirectArgumentDstCounterOffset);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST, frameIndex);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer[frameIndex].get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST));

	DebugGeometryRender& debug = DebugGeometryRender::instance();
	for (const auto& aabb : _boundingBoxies) {
		debug.debugDrawCube(aabb.center(), Quaternion::identity, aabb.size());
	}
}

void StaticMultiMeshRCG::destroy() {
	for (int i = 0; i < FrameCount; ++i) {
		_indirectArgumentSourceBuffer[i].destroy();
		_indirectArgumentDstBuffer[i].destroy();
	}

	delete[] _gpuDrivenInstanceCulledBuffer;
	_uavCounterReset.destroy();

	_cullingComputeRootSignature.destroy();
	_cullingComputeState.destroy();
	_setupCommandComputeRootSignature.destroy();
	_setupCommandComputeState.destroy();
	_commandSignature.destroy();

	DescriptorHeapManager& _descriptorHeapManager = DescriptorHeapManager::instance();
	_descriptorHeapManager.discardShaderResourceView(srv);

	rootSignature.destroy();
	pipelineState.destroy();
	cb.shutdown();

	_indirectArgumentOffsetsBuffer.destroy();
	_gpuDrivenInstanceMatrixBuffer.destroy();
	_gpuCullingCameraConstantBuffers.shutdown();
}

void StaticMultiMeshRCG::updateCullingCameraInfo(const Camera & camera, uint32 frameIndex) {
	GpuCullingCameraConstant gpuCullingConstant;
	gpuCullingConstant.cameraPosition = camera.getPosition();

	for (uint32 i = 0; i < 4; ++i) {
		gpuCullingConstant.frustumPlanes[i] = camera.getFrustumPlaneNormal(i);
	}

	_gpuCullingCameraConstantBuffers.writeBufferData(&gpuCullingConstant, sizeof(gpuCullingConstant), 0);
	_gpuCullingCameraConstantBuffers.flashBufferData(frameIndex);
}

void StaticMultiMeshRCG::culledBufferBarrier(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, uint32 frameIndex) {
	VectorArray<D3D12_RESOURCE_BARRIER> barriers(_meshCount);
	for (uint32 i = 0; i < _meshCount; ++i) {
		uint32 index = frameIndex * _meshCount + i;
		barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[i].Transition.StateBefore = StateBefore;
		barriers[i].Transition.StateAfter = StateAfter;
		barriers[i].Transition.pResource = _gpuDrivenInstanceCulledBuffer[index].get();
	}

	commandList->ResourceBarrier(_meshCount, barriers.data());
}
