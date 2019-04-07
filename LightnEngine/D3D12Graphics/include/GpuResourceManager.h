#pragma once

#include <Utility.h>

struct ID3D12Device;
struct BufferView;
class MeshRenderSet;
class VertexShader;
class PixelShader;
class Texture2D;
class SharedMaterial;
class CommandContext;

class GpuResourceManager :public Singleton<GpuResourceManager> {
public:
	~GpuResourceManager();

	void createSharedMaterial(ID3D12Device* device, const SharedMaterialCreateSettings& settings);
	void createTextures(ID3D12Device* device, CommandContext& commandContext, const VectorArray<String>& settings);
	void createMeshSets(ID3D12Device* device, CommandContext& commandContext, const VectorArray<String>& fileName);

	void loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial);
	void loadTexture(const String& textureName, RefPtr<Texture2D>& dstTexture);
	void loadMeshSets(const String& meshName, RefPtr<MeshRenderSet>& dstMeshSet);

	void shutdown();

private:
	UnorderedMap<String, UniquePtr<VertexShader>> _vertexShaders;
	UnorderedMap<String, UniquePtr<PixelShader>> _pixelShaders;
	UnorderedMap<String, UniquePtr<SharedMaterial>> _sharedMaterials;
	UnorderedMap<String, UniquePtr<Texture2D>> _textures;
	UnorderedMap<String, UniquePtr<MeshRenderSet>> _meshes;
};