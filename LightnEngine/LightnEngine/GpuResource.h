#pragma once

#include "stdafx.h"
#include "D3D12Helper.h"
#include "D3D12Util.h"
#include <vector>
#include "CommandContext.h"

//class CommandContext;
#include "ThirdParty/DDSTextureLoader12.h"

class GpuResource {
public:
	virtual ~GpuResource() {
		destroy();
	}

	void destroy() {
		_resource = nullptr;
	}

	ID3D12Resource* get() {
		return _resource.Get();
	}

	ID3D12Resource** getAdressOf() {
		return _resource.GetAddressOf();
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> _resource;
};

struct DepthTextureInfo {
	uint32 width;
	uint32 height;
	DXGI_FORMAT format;
};

struct TextureInfo {
	String name;
	uint32 width;
	uint32 height;
	uint32 mipLevels;
	void* dataPtr;
	DXGI_FORMAT format;
};

struct Vertex {
	float position[3];
	float color[4];
};

class Texture2D :public GpuResource {
public:
	//バックバッファからテクスチャを生成
	void createFromBackBuffer(IDXGISwapChain3* swapChain, uint32 index) {
		throwIfFailed(swapChain->GetBuffer(index, IID_PPV_ARGS(&_resource)));
	}

	//デプステクスチャとして生成
	void createDepth(ID3D12Device* device, const DepthTextureInfo& info) {
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
		depthOptimizedClearValue.Format = info.format;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		D3D12_RESOURCE_DESC depthBufferDesc = {};
		depthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthBufferDesc.Alignment = 0;
		depthBufferDesc.Width = info.width;
		depthBufferDesc.Height = info.height;
		depthBufferDesc.DepthOrArraySize = 1;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.Format = info.format;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		device->CreateCommittedResource(&LTND3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthBufferDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&_resource));
		NAME_D3D12_OBJECT(_resource);
	}

	//直ちにテクスチャを生成し、操作が完了するまでスレッドを停止する
	void createDirect(ID3D12Device* device, CommandContext* commandContext, const TextureInfo& info) {
		//テクスチャコピーコマンドを発行する用のアロケーターとリスト
		auto commandListSet = commandContext->requestCommandListSet();
		ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

		ComPtr<ID3D12Resource> textureUploadHeap;

		createDeferred(device, commandList, textureUploadHeap.ReleaseAndGetAddressOf(), info);

		//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
		commandContext->executeCommandList(commandList);
		commandContext->discardCommandListSet(commandListSet);

		//コピーが終わるまでアップロードヒープを破棄しない
		commandContext->waitForIdle();
	}

	//コマンドリストにテクスチャ生成コマンドを発行(発行のみで実行はしない)
	void createDeferred(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ID3D12Resource** uploadHeap, const TextureInfo& info) {
		UINT texturePixelSize = 4;

		//GPU読み出し専用テクスチャ本体を生成
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = info.format;
		textureDesc.Width = info.width;
		textureDesc.Height = info.height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		throwIfFailed(device->CreateCommittedResource(&LTND3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_resource)));

		UINT64 requiredSize = 0;
		device->GetCopyableFootprints(&_resource->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);

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
		throwIfFailed(device->CreateCommittedResource(&LTND3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &textureBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadHeap)));
		NAME_D3D12_OBJECT(_resource);

		//テクスチャデータをセット
		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = info.dataPtr;
		textureData.RowPitch = info.width * texturePixelSize;
		textureData.SlicePitch = textureData.RowPitch*info.height;

		//GPU読み出し専用バッファにコピーコマンドを発行
		updateSubresources(commandList, _resource.Get(), *uploadHeap, 0, 0, 1, &textureData);
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}

	void createDeferred2(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ID3D12Resource** uploadHeap, const String& textureName) {
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> subresouceData;
		throwIfFailed(DirectX::LoadDDSTextureFromFile(device, convertWString(textureName).c_str(), _resource.ReleaseAndGetAddressOf(), ddsData, subresouceData));

		const UINT subresouceSize = static_cast<UINT>(subresouceData.size());

		UINT64 requiredSize = 0;
		device->GetCopyableFootprints(&_resource->GetDesc(), 0, subresouceSize, 0, nullptr, nullptr, nullptr, &requiredSize);

		//テクスチャにデータをセットするためにCPUからも書き込み可能なバッファを生成
		D3D12_RESOURCE_DESC textureBufferDesc = {};
		textureBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		textureBufferDesc.Width = requiredSize;
		textureBufferDesc.Height = 1;
		textureBufferDesc.DepthOrArraySize = 1;
		textureBufferDesc.MipLevels = 1;
		textureBufferDesc.SampleDesc.Count = 1;
		textureBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		throwIfFailed(device->CreateCommittedResource(&LTND3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &textureBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadHeap)));

		//GPU読み出し専用バッファにコピーコマンドを発行
		updateSubresources(commandList, _resource.Get(), *uploadHeap, 0, 0, subresouceSize, subresouceData.data());
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}
};

class VertexBuffer :public GpuResource {
public:
	//直ちに頂点バッファを生成し、操作が完了するまでスレッドを停止する
	template <typename T>
	void createDirect(ID3D12Device* device, CommandContext* commandContext, const std::vector<T>& vertices) {
		ComPtr<ID3D12Resource> vertexUploadHeap;
		auto commandListSet = commandContext->requestCommandListSet();
		ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

		createDeferred<T>(device, commandList, vertexUploadHeap.ReleaseAndGetAddressOf(), vertices);

		//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
		commandContext->executeCommandList(commandList);
		commandContext->discardCommandListSet(commandListSet);

		//コピーが終わるまでアップロードヒープを破棄しない
		commandContext->waitForIdle();
	}

	//コマンドリストに頂点バッファ生成コマンドを発行(発行のみで実行はしない)
	template <typename T>
	void createDeferred(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ID3D12Resource** uploadHeap, const std::vector<T>& vertices) {
		const UINT64 vertexBufferSize = static_cast<UINT64>(sizeof(T)*vertices.size());

		D3D12_HEAP_PROPERTIES vertexUploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
		D3D12_HEAP_PROPERTIES vertexHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };

		//アップロード用バッファを生成
		D3D12_RESOURCE_DESC vertexBufferDesc = {};
		vertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		vertexBufferDesc.Width = vertexBufferSize;
		vertexBufferDesc.Height = 1;
		vertexBufferDesc.DepthOrArraySize = 1;
		vertexBufferDesc.MipLevels = 1;
		vertexBufferDesc.SampleDesc.Count = 1;
		vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		throwIfFailed(device->CreateCommittedResource(
			&vertexUploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadHeap)
		));

		throwIfFailed(device->CreateCommittedResource(
			&vertexHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&_resource)
		));

		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData = vertices.data();
		vertexData.RowPitch = vertexBufferSize;
		vertexData.SlicePitch = vertexData.RowPitch;

		updateSubresources<1>(commandList, _resource.Get(), *uploadHeap, 0, 0, 1, &vertexData);
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		_vertexBufferView.BufferLocation = _resource->GetGPUVirtualAddress();
		_vertexBufferView.StrideInBytes = sizeof(T);
		_vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);
	}

	D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;
};

class IndexBuffer :public GpuResource {
public:
	void createDirect(ID3D12Device* device, CommandContext* commandContext, const std::vector<UINT32>& indices) {
		ComPtr<ID3D12Resource> indexUploadHeap;

		auto commandListSet = commandContext->requestCommandListSet();
		ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

		createDeferred(device, commandList, indexUploadHeap.ReleaseAndGetAddressOf(), indices);

		//コマンド実行(アップロードバッファのテクスチャからGPU読み書き限定バッファにコピー)
		commandContext->executeCommandList(commandList);
		commandContext->discardCommandListSet(commandListSet);

		//コピーが終わるまでアップロードヒープを破棄しない
		commandContext->waitForIdle();
	}

	//コマンドリストにインデックスバッファ生成コマンドを発行(発行のみで実行はしない)
	void createDeferred(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ID3D12Resource** uploadHeap, const std::vector<UINT32>& indices) {
		const UINT64 indexBufferSize = static_cast<UINT64>(sizeof(UINT32)*indices.size());

		D3D12_HEAP_PROPERTIES indexHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };
		D3D12_HEAP_PROPERTIES indexUploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };

		D3D12_RESOURCE_DESC indexBufferDesc = {};
		indexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		indexBufferDesc.Width = indexBufferSize;
		indexBufferDesc.Height = 1;
		indexBufferDesc.DepthOrArraySize = 1;
		indexBufferDesc.MipLevels = 1;
		indexBufferDesc.SampleDesc.Count = 1;
		indexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		throwIfFailed(device->CreateCommittedResource(
			&indexUploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadHeap)
		));

		throwIfFailed(device->CreateCommittedResource(
			&indexHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&_resource)
		));

		D3D12_SUBRESOURCE_DATA indexData = {};
		indexData.pData = indices.data();
		indexData.RowPitch = indexBufferSize;
		indexData.SlicePitch = indexData.RowPitch;

		updateSubresources<1>(commandList, _resource.Get(), *uploadHeap, 0, 0, 1, &indexData);
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		_indexBufferView.BufferLocation = _resource->GetGPUVirtualAddress();
		_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		_indexBufferView.SizeInBytes = static_cast<UINT>(indexBufferSize);
	}

	D3D12_INDEX_BUFFER_VIEW _indexBufferView;
};

class ConstantBuffer :public GpuResource {
public:

	void create(ID3D12Device* device, uint32 size) {
		D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_UPLOAD };

		//256バイトでアライン
		const UINT64 constantBufferSize = (size + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = constantBufferSize;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.SampleDesc.Count = 1;

		device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_resource));

		NAME_D3D12_OBJECT(_resource);
		throwIfFailed(_resource->Map(0, nullptr, reinterpret_cast<void**>(&dataPtr)));
	}

	void writeData(const void* bufferPtr, uint32 size, uint32 startOffset = 0) {
		memcpy(dataPtr, reinterpret_cast<const byte*>(bufferPtr) + startOffset, size);
	}

	void* dataPtr;
};
