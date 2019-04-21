#include <Utility.h>
#include <PipelineState.h>
#include <SharedMaterial.h>
#include <GpuResource.h>
#include <MeshRenderSet.h>
#include <RenderableEntity.h>

struct GpuResourceDataPool {
	~GpuResourceDataPool() {
		shutdown();
	}

	void shutdown() {
		_vertexShaders.clear();
		_pixelShaders.clear();
		_pipelineStates.clear();
		_rootSignatures.clear();
		_sharedMaterials.clear();
		_textures.clear();
		_meshes.clear();
		_renderList.clear();
	}

	UnorderedMap<String, VertexShader> _vertexShaders;
	UnorderedMap<String, PixelShader> _pixelShaders;
	UnorderedMap<String, PipelineState> _pipelineStates;
	UnorderedMap<String, RootSignature> _rootSignatures;
	UnorderedMap<String, SharedMaterial> _sharedMaterials;
	UnorderedMap<String, Texture2D> _textures;
	UnorderedMap<String, MeshRenderSet> _meshes;

	ListArray<StaticSingleMeshRender> _renderList;
};