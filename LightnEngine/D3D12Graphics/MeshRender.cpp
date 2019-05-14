#include "MeshRender.h"
#include "SharedMaterial.h"
#include "GpuResource.h"
#include "DescriptorHeap.h"
#include "GpuResourceManager.h"

StaticSingleMeshRCG::StaticSingleMeshRCG(
	const RefVertexBufferView& vertexBufferView,
	const RefIndexBufferView& indexBufferView,
	const VectorArray<MaterialSlot>& materialSlots) :
	_vertexBufferView(vertexBufferView),
	_indexBufferView(indexBufferView),
	_materialSlotSize(materialSlots.size()),
	_worldMatrix(Matrix4::identity) {

	//このインスタンスの末尾にマテリアルのデータをコピー
	//インスタンス生成時に適切なメモリを確保しておかないと即死亡！！！
	RefPtr<MaterialSlot> endPtr = getFirstMatrialPtr();
	memcpy(endPtr, materialSlots.data(), _materialSlotSize * sizeof(MaterialSlot));
}

void StaticSingleMeshRCG::setupRenderCommand(RenderSettings& settings) const {
	RefPtr<ID3D12GraphicsCommandList> commandList = settings.commandList;
	const uint32 frameIndex = settings.frameIndex;

	commandList->IASetVertexBuffers(0, 1, &_vertexBufferView.view);
	commandList->IASetIndexBuffer(&_indexBufferView.view);

	//このインスタンスのすぐ後ろにマテリアルのコマンドが詰まっているのでまずは最初を取り出す
	RefPtr<MaterialSlot> material = getFirstMatrialPtr();

	for (size_t i = 0; i < _materialSlotSize; ++i) {
		const RefSharedMaterial& mat = material->material;

		commandList->SetPipelineState(mat.pipelineState.pipelineState);
		commandList->SetGraphicsRootSignature(mat.rootSignature.rootSignature);

		//ピクセルシェーダーリソースビューをマップ
		const RefBufferView& srvPixel = mat.srvPixel;
		if (srvPixel.isEnable()) {
			commandList->SetGraphicsRootDescriptorTable(srvPixel.descriptorIndex, srvPixel.gpuHandle);
		}

		//ピクセル定数バッファをマップ
		if (mat.pixelConstantViews.isEnableBuffers()) {
			const RefBufferView& view = mat.pixelConstantViews.views[frameIndex];
			commandList->SetGraphicsRootDescriptorTable(view.descriptorIndex, view.gpuHandle);
		}

		//for (uint32 i = 0; i < mat.pixelRoot32bitConstantCount; ++i) {
		//	commandList->SetGraphicsRoot32BitConstants(mat.pixelRoot32bitConstantIndex + i, settings.pixelRoot32bitConstants[i].second / 4, settings.pixelRoot32bitConstants[i].first, 0);
		//}

		//頂点シェーダーリソースをマップ
		const RefBufferView& srvVertex = mat.srvVertex;
		if (srvVertex.isEnable()) {
			commandList->SetGraphicsRootDescriptorTable(srvVertex.descriptorIndex, srvVertex.gpuHandle);
		}

		//頂点定数バッファをマップ
		if (mat.vertexConstantViews.isEnableBuffers()) {
			const RefBufferView& view = mat.vertexConstantViews.views[frameIndex];
			commandList->SetGraphicsRootDescriptorTable(view.descriptorIndex, view.gpuHandle);
		}

		//シングルスタティックメッシュ用のワールド行列をマップ
		commandList->SetGraphicsRoot32BitConstants(mat.vertexRoot32bitConstantIndex, static_cast<uint32>(sizeof(_worldMatrix) / 4), &_worldMatrix, 0);

		//ドローコール
		commandList->IASetPrimitiveTopology(mat.topology);
		commandList->DrawIndexedInstanced(material->range.indexCount, 1, material->range.indexOffset, 0, 0);

		//ポインタをインクリメントして次のマテリアルを参照
		++material;
	}
}

void StaticSingleMeshRCG::updateWorldMatrix(const Matrix4 & worldMatrix) {
	_worldMatrix = worldMatrix;
}

void StaticMultiMeshRCG::create(RefPtr<ID3D12Device> device, RefPtr<CommandContext> commandContext, const VectorArray<IndirectMeshInfo> & meshes, const String& materialName) {
	DescriptorHeapManager& descriptorHeapManager = DescriptorHeapManager::instance();
	GpuResourceManager& gpuResourceManager = GpuResourceManager::instance();

	gpuResourceManager.loadSharedMaterial(materialName, &_material);

	//描画元情報からGPUカリングとIndirect描画に必要な情報をまとめる
	_indirectArgumentCount = static_cast<uint32>(meshes.size());
	_indirectMeshes.resize(_indirectArgumentCount);
	for (size_t i = 0; i < _indirectMeshes.size(); ++i) {
		_indirectMeshes[i].vertexBufferView = meshes[i].vertexAndIndexBuffer->vertexBuffer._vertexBufferView;
		_indirectMeshes[i].indexBufferView = meshes[i].vertexAndIndexBuffer->indexBuffer._indexBufferView;
		_indirectMeshes[i].indexCount = meshes[i].vertexAndIndexBuffer->indexBuffer._indexCount;
		_indirectMeshes[i].instanceCount = meshes[i].maxInstanceCount;
		_indirectMeshes[i].counterOffset = AlignForUavCounter(meshes[i].maxInstanceCount * sizeof(PerInstanceVertex));
	}

	//コマンドシグネチャ生成
	{
		_gpuDrivenInstanceCulledBuffer = new GpuBuffer[_indirectArgumentCount * FrameCount];
		_indirectArgumentDstCounterOffset = AlignForUavCounter(_indirectArgumentCount * sizeof(IndirectCommand));

		// Each command consists of a CBV update and a DrawInstanced call.
		VectorArray<D3D12_INDIRECT_ARGUMENT_DESC> argumentDescs(4);
		argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
		argumentDescs[0].VertexBuffer.Slot = 0;
		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
		argumentDescs[1].VertexBuffer.Slot = 0;
		argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
		argumentDescs[2].VertexBuffer.Slot = 1;
		argumentDescs[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		_commandSignature.create(device, sizeof(IndirectCommand), argumentDescs);
	}

	//GPUカリングステート
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
		uavRange.NumDescriptors = _indirectArgumentCount;
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
		computeShader.create("Shaders/GpuCulling_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);//DynamicIndexingを使用

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = _cullingComputeRootSignature._rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();
		_cullingComputeState.createCompute(device, computePsoDesc);
	}

	//IndirectCommandステート
	{
		D3D12_DESCRIPTOR_RANGE1 srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = _indirectArgumentCount;
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
		computeShader.create("Shaders/SetupIndirectCommand_cs.hlsl", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES);//DynamicIndexingを使用

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = _setupCommandComputeRootSignature._rootSignature.Get();
		computePsoDesc.CS = computeShader.getByteCode();
		_setupCommandComputeState.createCompute(device, computePsoDesc);
	}

	ComPtr<ID3D12Resource> inCommandUploadBuffers;
	ComPtr<ID3D12Resource> indirectCommandUploadBuffers[FrameCount];

	auto commandListSet = commandContext->requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	//カリング前のシーンに配置されているオブジェクトの行列バッファ
	uint32 totalMaxInstanceCount = 0;
	for (const auto& indirectMesh : meshes) {
		totalMaxInstanceCount += indirectMesh.maxInstanceCount;
	}

	size_t mergedMatricsOffset = 0;
	VectorArray<ObjectInfo> mergedMatrices(totalMaxInstanceCount);
	for (uint32 i = 0; i < _indirectArgumentCount; ++i) {
		memcpy(mergedMatrices.data() + mergedMatricsOffset, meshes[i].matrices.data(), meshes[i].matrices.size() * sizeof(ObjectInfo));
		mergedMatricsOffset += meshes[i].matrices.size();
	}

	//トータルのインスタンス数を256の数(Thread X Num)でアラインして何回Dispatchするか決める
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

	//インスタンス用行列頂点バッファとそのUAVを生成
	VectorArray<D3D12_BUFFER_UAV> gpuDrivenInstanceCulledBufferUavs(_indirectArgumentCount);
	VectorArray<D3D12_BUFFER_SRV> gpuDrivenInstanceCulledBufferSrvs(_indirectArgumentCount);

	for (size_t i = 0; i < gpuDrivenInstanceCulledBufferUavs.size(); ++i) {
		D3D12_BUFFER_UAV& bufferUav = gpuDrivenInstanceCulledBufferUavs[i];
		bufferUav.FirstElement = 0;
		bufferUav.NumElements = _indirectMeshes[i].instanceCount;
		bufferUav.StructureByteStride = sizeof(PerInstanceVertex);
		bufferUav.CounterOffsetInBytes = _indirectMeshes[i].counterOffset;

		//ByteAddressBufferとして読み込んでAppendStructuredBufferのカウンタの値を読み取る
		constexpr UINT CountLength = 1;
		D3D12_BUFFER_SRV& bufferSrv = gpuDrivenInstanceCulledBufferSrvs[i];
		bufferSrv.FirstElement = 0;
		bufferSrv.NumElements =static_cast<UINT>(bufferUav.CounterOffsetInBytes / sizeof(UINT) + CountLength);//4byteオフセットの数で指定する
		bufferSrv.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	}

	for (uint32 i = 0; i < FrameCount; ++i) {
		VectorArray<RefPtr<ID3D12Resource>> ppCulledBuffers(_indirectArgumentCount);

		for (uint32 j = 0; j < _indirectArgumentCount; ++j) {
			uint32 index = i * _indirectArgumentCount + j;
			_gpuDrivenInstanceCulledBuffer[index].createDirectGpuOnlyEmpty(device, _indirectMeshes[j].counterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			ppCulledBuffers[j] = _gpuDrivenInstanceCulledBuffer[index].get();
		}

		descriptorHeapManager.createUnorederdAcsessView(ppCulledBuffers.data(), &_gpuDriventInstanceCulledUAV[i], _indirectArgumentCount, gpuDrivenInstanceCulledBufferUavs);
		descriptorHeapManager.createShaderResourceView(ppCulledBuffers.data(), &_gpuDriventInstanceCulledSRV[i], _indirectArgumentCount, gpuDrivenInstanceCulledBufferSrvs);
	}

	//ExecuteIndirectに渡すIndirectBufferを生成
	for (uint32 i = 0; i < FrameCount; ++i) {
		VectorArray<IndirectCommand> commands(_indirectArgumentCount);

		for (size_t j = 0; j < _indirectMeshes.size(); ++j) {
			const uint32 culledBufferIndex = i * _indirectArgumentCount + static_cast<uint32>(j);
			PerInstanceIndirectArgument& mesh = _indirectMeshes[j];

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

		_indirectArgumentSourceBuffer[i].createDeferredGpuOnly<IndirectCommand>(device, commandList, &indirectCommandUploadBuffers[i], commands);
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentSourceBuffer[i].get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		//GPUカリング後のIndirectBuffer
		_indirectArgumentDstBuffer[i].createDirectGpuOnlyEmpty(device, _indirectArgumentDstCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		//IndirectArgumentバッファのUAVを作成
		D3D12_BUFFER_UAV bufferUav;
		bufferUav.FirstElement = 0;
		bufferUav.NumElements = _indirectArgumentCount;
		bufferUav.StructureByteStride = sizeof(IndirectCommand);
		bufferUav.CounterOffsetInBytes = _indirectArgumentDstCounterOffset;

		descriptorHeapManager.createUnorederdAcsessView(_indirectArgumentDstBuffer[i].getAdressOf(), &_setupCommandUavView[i], 1, { bufferUav });
	}

	//CulledBufferのカウンタまでのバイトオフセットを格納するバッファ
	ComPtr<ID3D12Resource> offsetsUpload;
	VectorArray<uint32> counterOffsets(_indirectArgumentCount);
	for (size_t i = 0; i < counterOffsets.size(); ++i) {
		counterOffsets[i] = _indirectMeshes[i].counterOffset;
	}

	_indirectArgumentOffsetsBuffer.createDeferredGpuOnly<uint32>(device, commandList, &offsetsUpload, counterOffsets);

	//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
	commandContext->executeCommandList(commandList);
	commandContext->discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	commandContext->waitForIdle();

	//UAVカウンタをリセットするための(UINT)0のバッファを生成
	_uavCounterReset.createDirectCpuReadWrite<UINT>(device, { 0 });
}

void StaticMultiMeshRCG::onCompute(RefPtr<CommandContext> commandContext, uint32 frameIndex) {
	DescriptorHeapManager& _descriptorHeapManager = DescriptorHeapManager::instance();

	auto computeCommandListSet = commandContext->requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> computeCommandList = computeCommandListSet.commandList;

	//デスクリプタヒープをセット
	RefPtr<ID3D12DescriptorHeap> ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	computeCommandList->SetDescriptorHeaps(1, ppHeap);

	//GPUカリング
	{
		computeCommandList->SetPipelineState(_cullingComputeState._pipelineState.Get());
		computeCommandList->SetComputeRootSignature(_cullingComputeRootSignature._rootSignature.Get());

		computeCommandList->SetComputeRootDescriptorTable(0, _gpuDrivenInstanceMatrixView.gpuHandle);
		computeCommandList->SetComputeRootDescriptorTable(1, _gpuDriventInstanceCulledUAV[frameIndex].gpuHandle);
		computeCommandList->SetComputeRootConstantBufferView(2, _gpuCullingCameraConstantBuffers.constantBuffers[frameIndex][0]->_resource->GetGPUVirtualAddress());

		//AppendStructuredBufferのカウンタを0にリセットする
		for (uint32 i = 0; i < _indirectArgumentCount; ++i) {
			uint32 index = frameIndex * _indirectArgumentCount + i;
			computeCommandList->CopyBufferRegion(_gpuDrivenInstanceCulledBuffer[index].get(), _indirectMeshes[i].counterOffset, _uavCounterReset.get(), 0, sizeof(UINT));
		}

		culledBufferBarrier(computeCommandList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, frameIndex);

		computeCommandList->Dispatch(_gpuCullingDispatchCount, 1, 1);
	}

	//ExecuteIndirectコマンド構築
	{
		computeCommandList->SetPipelineState(_setupCommandComputeState._pipelineState.Get());
		computeCommandList->SetComputeRootSignature(_setupCommandComputeRootSignature._rootSignature.Get());

		culledBufferBarrier(computeCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, frameIndex);

		computeCommandList->SetComputeRootShaderResourceView(0, _indirectArgumentSourceBuffer[frameIndex].getGpuVirtualAddress());
		computeCommandList->SetComputeRootShaderResourceView(1, _indirectArgumentOffsetsBuffer.getGpuVirtualAddress());
		computeCommandList->SetComputeRootDescriptorTable(2, _gpuDriventInstanceCulledSRV[frameIndex].gpuHandle);
		computeCommandList->SetComputeRootDescriptorTable(3, _setupCommandUavView[frameIndex].gpuHandle);

		//AppendStructuredBufferのカウンタを0にリセットする
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

	_material->setupRenderCommand(settings);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, frameIndex);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer[frameIndex].get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

	//EXECUTION WARNING #1044: GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE
	//IndirectAtgumentバッファに含まれるバーテックスバッファを一度UAVとして扱うのでGPUデバッグレイヤー上でリソースの追跡ができないと警告
	commandList->ExecuteIndirect(
		_commandSignature._commandSignature.Get(),
		_indirectArgumentCount,
		_indirectArgumentDstBuffer[frameIndex].get(),
		0,
		_indirectArgumentDstBuffer[frameIndex].get(),
		_indirectArgumentDstCounterOffset);

	culledBufferBarrier(commandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST, frameIndex);
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_indirectArgumentDstBuffer[frameIndex].get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST));

}

void StaticMultiMeshRCG::destroy() {
	for (int i = 0; i < FrameCount; ++i) {
		_indirectArgumentSourceBuffer[i]._resource = nullptr;
		_indirectArgumentDstBuffer[i]._resource = nullptr;
	}

	delete[] _gpuDrivenInstanceCulledBuffer;
	_uavCounterReset._resource = nullptr;

	_cullingComputeRootSignature._rootSignature = nullptr;
	_cullingComputeState._pipelineState = nullptr;
	_setupCommandComputeRootSignature._rootSignature = nullptr;
	_setupCommandComputeState._pipelineState = nullptr;
	_commandSignature._commandSignature = nullptr;

	_indirectArgumentOffsetsBuffer.destroy();
	_gpuDrivenInstanceMatrixBuffer.destroy();
	_gpuCullingCameraConstantBuffers.shutdown();
}

void StaticMultiMeshRCG::updateCullingCameraInfo(const Camera& virtualCamera, uint32 frameIndex) {
	GpuCullingCameraConstant gpuCullingConstant;
	gpuCullingConstant.cameraPosition = virtualCamera.getPosition();

	for (uint32 i = 0; i < 4; ++i) {
		gpuCullingConstant.frustumPlanes[i] = virtualCamera.getFrustumPlaneNormal(i);
	}

	_gpuCullingCameraConstantBuffers.writeBufferData(&gpuCullingConstant, sizeof(gpuCullingConstant), 0);
	_gpuCullingCameraConstantBuffers.flashBufferData(frameIndex);
}

void StaticMultiMeshRCG::culledBufferBarrier(RefPtr<ID3D12GraphicsCommandList> commandList, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter, uint32 frameIndex) {
	VectorArray<D3D12_RESOURCE_BARRIER> barriers(_indirectArgumentCount);
	for (uint32 i = 0; i < _indirectArgumentCount; ++i) {
		uint32 index = frameIndex * _indirectArgumentCount + i;
		barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[i].Transition.StateBefore = StateBefore;
		barriers[i].Transition.StateAfter = StateAfter;
		barriers[i].Transition.pResource = _gpuDrivenInstanceCulledBuffer[index].get();
	}

	commandList->ResourceBarrier(_indirectArgumentCount, barriers.data());
}
