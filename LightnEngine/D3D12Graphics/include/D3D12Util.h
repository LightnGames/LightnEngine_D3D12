#pragma once
#include <d3d12.h>

//ヒープ属性
struct LTND3D12_HEAP_PROPERTIES :public D3D12_HEAP_PROPERTIES {
	LTND3D12_HEAP_PROPERTIES() = default;
	explicit LTND3D12_HEAP_PROPERTIES(const D3D12_HEAP_PROPERTIES& o) :D3D12_HEAP_PROPERTIES(o) {}

	LTND3D12_HEAP_PROPERTIES(
		D3D12_CPU_PAGE_PROPERTY cpuPageProperty,
		D3D12_MEMORY_POOL memoryPoolPreference,
		UINT creationNodeMask = 1,
		UINT nodeMask = 1) {
		Type = D3D12_HEAP_TYPE_CUSTOM;
		CPUPageProperty = cpuPageProperty;
		MemoryPoolPreference = memoryPoolPreference;
		CreationNodeMask = creationNodeMask;
		VisibleNodeMask = nodeMask;
	}

	explicit LTND3D12_HEAP_PROPERTIES(
		D3D12_HEAP_TYPE type,
		UINT creationNodeMask = 1,
		UINT nodeMask = 1) {
		Type = type;
		CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		CreationNodeMask = creationNodeMask;
		VisibleNodeMask = nodeMask;
	}
};

//リソースバリア
struct LTND3D12_RESOURCE_BARRIER :public D3D12_RESOURCE_BARRIER {
	LTND3D12_RESOURCE_BARRIER() = default;
	explicit LTND3D12_RESOURCE_BARRIER(const D3D12_RESOURCE_BARRIER& o):D3D12_RESOURCE_BARRIER(o){}

	static inline LTND3D12_RESOURCE_BARRIER transition(
		_In_ ID3D12Resource* pResource,
		D3D12_RESOURCE_STATES stateBefore,
		D3D12_RESOURCE_STATES stateAfter,
		UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE) {
		LTND3D12_RESOURCE_BARRIER result = {};
		D3D12_RESOURCE_BARRIER &barrier = result;
		result.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		result.Flags = flags;
		barrier.Transition.pResource = pResource;
		barrier.Transition.StateBefore = stateBefore;
		barrier.Transition.StateAfter = stateAfter;
		barrier.Transition.Subresource = subresource;

		return result;
	}

	static inline LTND3D12_RESOURCE_BARRIER aliasing(
		_In_ ID3D12Resource* pResourceBefore,
		_In_ ID3D12Resource* pResourceAfter) {
		LTND3D12_RESOURCE_BARRIER result = {};
		D3D12_RESOURCE_BARRIER &barrier = result;
		result.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
		barrier.Aliasing.pResourceBefore = pResourceBefore;
		barrier.Aliasing.pResourceAfter = pResourceAfter;
		
		return result;
	}

	static inline LTND3D12_RESOURCE_BARRIER uav(
		_In_ ID3D12Resource* pResource) {
		LTND3D12_RESOURCE_BARRIER result = {};
		result.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		result.UAV.pResource = pResource;
		return result;
	}
};

//テクスチャコピー用の定義
struct LTND3D12_TEXTURE_COPY_LOCATION :public D3D12_TEXTURE_COPY_LOCATION {
	LTND3D12_TEXTURE_COPY_LOCATION() = default;

	explicit LTND3D12_TEXTURE_COPY_LOCATION(const D3D12_TEXTURE_COPY_LOCATION& o):
		D3D12_TEXTURE_COPY_LOCATION(o){ }

	LTND3D12_TEXTURE_COPY_LOCATION(_In_ ID3D12Resource* pRes) {
		pResource = pRes;
		Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		PlacedFootprint = {};
	}

	LTND3D12_TEXTURE_COPY_LOCATION(_In_ ID3D12Resource* pRes, D3D12_PLACED_SUBRESOURCE_FOOTPRINT const& footprint) {
		pResource = pRes;
		Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		PlacedFootprint = footprint;
	}

	LTND3D12_TEXTURE_COPY_LOCATION(_In_ ID3D12Resource* pRes, UINT sub) {
		pResource = pRes;
		Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		SubresourceIndex = sub;
	}

};

inline void memcpySubresources(
	_In_ const D3D12_MEMCPY_DEST* pDest,
	_In_ const D3D12_SUBRESOURCE_DATA* pSrc,
	SIZE_T rowSizeInBytes,
	UINT numRows,
	UINT numSlices) {
	for (UINT z = 0; z < numSlices; ++z) {
		BYTE* pDestSlice = reinterpret_cast<BYTE*>(pDest->pData) + pDest->SlicePitch*z;
		const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(pSrc->pData) + pSrc->SlicePitch*z;
		for (UINT y = 0; y < numRows; ++y) {
			memcpy(pDestSlice + pDest->RowPitch*y,
				pSrcSlice + pSrc->RowPitch*y, rowSizeInBytes);
		}
	}
}

inline UINT64 updateSubresources(
	_In_ ID3D12GraphicsCommandList* pCmdList,
	_In_ ID3D12Resource* pDestinationResource,
	_In_ ID3D12Resource* pIntermediate,
	_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT firstSubresources,
	_In_range_(0, D3D12_REQ_SUBRESOURCES - firstSubresources) UINT numSubresources,
	UINT64 requiredSize,
	_In_reads_(numSubresources) const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
	_In_reads_(numSubresources) const UINT* pNumRows,
	_In_reads_(numSubresources) const UINT64* pRowSizesInBytes,
	_In_reads_(numSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) {

	auto intermediateDesc = pIntermediate->GetDesc();
	auto destinationDesc = pDestinationResource->GetDesc();
	if (intermediateDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
		intermediateDesc.Width<requiredSize + pLayouts[0].Offset ||
		requiredSize > SIZE_T(-1) ||
		(destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && (firstSubresources != 0 || numSubresources != 1))) {
		return 0;
	}

	BYTE* pData;
	HRESULT hr = pIntermediate->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	if (FAILED(hr)) {
		return 0;
	}

	//メモリコピー実行
	for (UINT i = 0; i < numSubresources; ++i) {
		if (pRowSizesInBytes[i] > SIZE_T(-1)) {
			return 0;
		}

		D3D12_MEMCPY_DEST destData = { pData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch, SIZE_T(pLayouts[i].Footprint.RowPitch)*SIZE_T(pNumRows[i]) };
		memcpySubresources(&destData, &pSrcData[i], static_cast<SIZE_T>(pRowSizesInBytes[i]), pNumRows[i], pLayouts[i].Footprint.Depth);
	}
	pIntermediate->Unmap(0, nullptr);

	if (destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
		pCmdList->CopyBufferRegion(pDestinationResource, 0, pIntermediate, pLayouts[0].Offset, pLayouts[0].Footprint.Width);
	}
	else {
		for (UINT i = 0; i < numSubresources; ++i) {
			LTND3D12_TEXTURE_COPY_LOCATION dst(pDestinationResource, i + firstSubresources);
			LTND3D12_TEXTURE_COPY_LOCATION src(pIntermediate, pLayouts[i]);
			pCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		}
	}

	return requiredSize;
}

//ヒープ領域を新たに確保してから更新
inline UINT64 updateSubresources(
	_In_ ID3D12GraphicsCommandList* pCmdList,
	_In_ ID3D12Resource* pDestinationResource,
	_In_ ID3D12Resource* pIntermediate,
	UINT64 intermediateOffset,
	_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT firstSubresources,
	_In_range_(0, D3D12_REQ_SUBRESOURCES - firstSubresources) UINT numSubresources,
	_In_reads_(numSubresources) D3D12_SUBRESOURCE_DATA* pSrcData) {

	UINT64 requiredSize = 0;
	UINT64 memToAlloc = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)) * numSubresources;
	if (memToAlloc > SIZE_MAX) {
		return 0;
	}

	void* pMem = HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(memToAlloc));
	if (pMem == nullptr) {
		return 0;
	}

	auto pLayouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
	UINT64* pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayouts + numSubresources);
	UINT* pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + numSubresources);

	auto desc = pDestinationResource->GetDesc();
	ID3D12Device* pDevice = nullptr;
	pDestinationResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
	pDevice->GetCopyableFootprints(&desc, firstSubresources, numSubresources, intermediateOffset, pLayouts, pNumRows, pRowSizesInBytes, &requiredSize);
	pDevice->Release();

	UINT64 result = updateSubresources(pCmdList, pDestinationResource, pIntermediate, firstSubresources, numSubresources, requiredSize, pLayouts, pNumRows, pRowSizesInBytes, pSrcData);
	HeapFree(GetProcessHeap(), 0, pMem);
	return result;
}

//スタック領域から更新
template<UINT MaxSubresources>
inline UINT64 updateSubresources(
	_In_ ID3D12GraphicsCommandList* pCmdList,
	_In_ ID3D12Resource* pDestinationResource,
	_In_ ID3D12Resource* pIntermediate,
	UINT64 intermediateOffset,
	_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT firstSubresources,
	_In_range_(0, D3D12_REQ_SUBRESOURCES - firstSubresources) UINT numSubresources,
	_In_reads_(numSubresources) D3D12_SUBRESOURCE_DATA* pSrcData) {

	UINT64 requiredSize = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[MaxSubresources];
	UINT numRows[MaxSubresources];
	UINT64 rowSizesInBytes[MaxSubresources];

	auto desc = pDestinationResource->GetDesc();
	ID3D12Device* pDevice;
	pDestinationResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
	pDevice->GetCopyableFootprints(&desc, firstSubresources, numSubresources, intermediateOffset, layouts, numRows, rowSizesInBytes, &requiredSize);
	pDevice->Release();

	return updateSubresources(pCmdList, pDestinationResource, pIntermediate, firstSubresources, numSubresources, requiredSize, layouts, numRows, rowSizesInBytes, pSrcData);
}