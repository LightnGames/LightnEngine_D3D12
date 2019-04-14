#include <Utility.h>
#include <PipelineState.h>
#include <SharedMaterial.h>
#include <GpuResource.h>
#include <MeshRenderSet.h>

struct GpuResourceDataPool {
	~GpuResourceDataPool() {
		shutdown();
	}

	void shutdown() {
		_vertexShaders.clear();
		_pixelShaders.clear();
		_sharedMaterials.clear();
		_textures.clear();
		_meshes.clear();
	}

	UnorderedMap<String, VertexShader> _vertexShaders;
	UnorderedMap<String, PixelShader> _pixelShaders;
	UnorderedMap<String, SharedMaterial> _sharedMaterials;
	UnorderedMap<String, Texture2D> _textures;
	UnorderedMap<String, MeshRenderSet> _meshes;
};