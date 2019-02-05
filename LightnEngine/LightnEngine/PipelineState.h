#pragma once
#include <d3d12.h>
#include <vector>

struct VertexShader {

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayouts;
	std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
	D3D12_SHADER_BYTECODE byteCode;
};

struct PixelShader {

};

struct RasterRizerState {

};

class PipelineState {
public:
	void create(ID3D12Device* device, VertexShader* vertexShader, PixelShader* pixelShader, RasterRizerState* rasterState) {

	}

	ID3D12PipelineState* pipeLineState;
};