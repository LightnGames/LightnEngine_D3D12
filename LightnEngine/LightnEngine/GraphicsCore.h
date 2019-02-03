#pragma once
#include "Utility.h"
#include "stdafx.h"
#include <vector>
#include <memory>

#define DEBUG

using namespace Microsoft::WRL;

class DescriptorHeapManager;
struct BufferView;
class FrameResource;
class CommandQueue;
class CommandContext;
class VertexBuffer;
class IndexBuffer;
class Texture2D;

class GraphicsCore {
public:
	GraphicsCore();
	~GraphicsCore();

	void onInit(HWND hwnd);
	void onUpdate();
	void onRender();
	void onDestroy();

public:
	GETTER(UINT, width);
	GETTER(UINT, height);

private:

	//éüÇÃÉtÉåÅ[ÉÄÇ‹Ç≈ë“ã@
	void moveToNextFrame();

	static const UINT TextureWidth = 256;
	static const UINT TextureHeight = 256;
	static const UINT TexturePixelSize = 4;

	std::vector<UINT8> generateTextureData() {
		const UINT rowPitch = TextureWidth * TexturePixelSize;
		const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
		const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
		const UINT textureSize = rowPitch * TextureHeight;

		std::vector<UINT8> data(textureSize);
		UINT8* pData = &data[0];

		for (UINT n = 0; n < textureSize; n += TexturePixelSize) {
			UINT x = n % rowPitch;
			UINT y = n / rowPitch;
			UINT i = x / cellPitch;
			UINT j = y / cellHeight;

			if (i % 2 == j % 2) {
				pData[n] = 0x00;        // R
				pData[n + 1] = 0x00;    // G
				pData[n + 2] = 0x88;    // B
				pData[n + 3] = 0xff;    // A
			}
			else {
				pData[n] = 0x88;        // R
				pData[n + 1] = 0x00;    // G
				pData[n + 2] = 0x00;    // B
				pData[n + 3] = 0x00;    // A
			}
		}

		return data;
	}

	struct Vertex {
		float position[3];
		float color[4];
	};

	UINT _frameIndex;

	D3D12_VIEWPORT _viewPort;
	D3D12_RECT _scissorRect;

	ComPtr<IDXGISwapChain3> _swapChain;
	ComPtr<ID3D12Device> _device;
	ComPtr<ID3D12RootSignature> _rootSignature;
	ComPtr<ID3D12PipelineState> _pipelineState;
	Texture2D* _depthStencil;

	BufferView* _textureSrv;
	BufferView* _dsv;

	SceneConstantBuffer _constantBufferData;
	VertexBuffer* _vertexBuffer;
	IndexBuffer* _indexBuffer;
	Texture2D* _texture;
	CommandContext* _commandContext;
	FrameResource* _frameResources[FrameCount];
	FrameResource* _currentFrameResource;
	DescriptorHeapManager* _descriptorHeapManager;
};

