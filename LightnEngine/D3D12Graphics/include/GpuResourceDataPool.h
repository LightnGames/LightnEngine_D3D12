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
		sharedMaterials.clear();
		textures.clear();
		vertexAndIndexBuffers.clear();
	}

	UnorderedMap<String, VertexShader> vertexShaders;
	UnorderedMap<String, PixelShader> pixelShaders;
	UnorderedMap<String, PipelineState> pipelineStates;
	UnorderedMap<String, RootSignature> rootSignatures;
	UnorderedMap<String, SingleMeshRenderPass> sharedMaterials;
	UnorderedMap<String, Texture2D> textures;
	UnorderedMap<String, VertexAndIndexBuffer> vertexAndIndexBuffers;
};