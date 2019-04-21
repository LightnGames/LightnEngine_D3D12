#include <Utility.h>
#include <PipelineState.h>
#include <SharedMaterial.h>
#include <GpuResource.h>
#include <RenderableEntity.h>

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
		renderLists.clear();
	}

	UnorderedMap<String, VertexShader> vertexShaders;
	UnorderedMap<String, PixelShader> pixelShaders;
	UnorderedMap<String, PipelineState> pipelineStates;
	UnorderedMap<String, RootSignature> rootSignatures;
	UnorderedMap<String, SharedMaterial> sharedMaterials;
	UnorderedMap<String, Texture2D> textures;
	UnorderedMap<String, VertexAndIndexBuffer> vertexAndIndexBuffers;

	ListArray<StaticSingleMeshRender> renderLists;
};