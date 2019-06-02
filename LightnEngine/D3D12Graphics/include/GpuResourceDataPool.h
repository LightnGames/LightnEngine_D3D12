#include <Utility.h>
#include <PipelineState.h>
#include <SharedMaterial.h>
#include <GpuResource.h>

struct GpuResourceDataPool {
	~GpuResourceDataPool() {
		shutdown();
	}

	void shutdown() {
		vertexShaders.clear();
		pixelShaders.clear();
		pipelineStates.clear();
		rootSignatures.clear();
		textures.clear();
		vertexAndIndexBuffers.clear();
		constantBuffers.clear();
		bufferViews.clear();
		gpuBuffers.clear();
		gpuDynamicBuffers.clear();
	}

	UnorderedMap<String, VertexShader> vertexShaders;
	UnorderedMap<String, PixelShader> pixelShaders;
	UnorderedMap<String, PipelineState> pipelineStates;
	UnorderedMap<String, RootSignature> rootSignatures;
	UnorderedMap<String, Texture2D> textures;
	UnorderedMap<String, VertexAndIndexBuffer> vertexAndIndexBuffers;
	UnorderedMap<String, ConstantBuffer> constantBuffers;
	UnorderedMap<String, BufferView> bufferViews;
	UnorderedMap<String, GpuBuffer> gpuBuffers;
	UnorderedMap<String, GpuBufferDynamic> gpuDynamicBuffers;
};