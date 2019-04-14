#pragma once

#include <Utility.h>
#include <RenderableEntity.h>

struct ID3D12Device;
struct BufferView;
struct IRenderableEntity;
class MeshRenderSet;
class VertexShader;
class PixelShader;
class Texture2D;
class SharedMaterial;
class CommandContext;
class StaticSingleMeshRender;

class GpuResourceManager :public Singleton<GpuResourceManager> {
public:
	~GpuResourceManager();

	void createSharedMaterial(ID3D12Device* device, const SharedMaterialCreateSettings& settings);
	void createTextures(ID3D12Device* device, CommandContext& commandContext, const VectorArray<String>& settings);
	void createMeshSets(ID3D12Device* device, CommandContext& commandContext, const VectorArray<String>& fileName);

	void loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial);
	void loadTexture(const String& textureName, RefPtr<Texture2D>& dstTexture);
	void loadMeshSets(const String& meshName, RefPtr<MeshRenderSet>& dstMeshSet);

	RefPtr<StaticSingleMeshRender> createStaticSingleMeshRender(const String& name);
	void removeStaticSingleMeshRender(RefPtr<StaticSingleMeshRender> render);

	void shutdown();

	const ListArray<UniquePtr<IRenderableEntity>>& getMeshes() const;

private:
	UnorderedMap<String, UniquePtr<VertexShader>> _vertexShaders;
	UnorderedMap<String, UniquePtr<PixelShader>> _pixelShaders;
	UnorderedMap<String, UniquePtr<SharedMaterial>> _sharedMaterials;
	UnorderedMap<String, UniquePtr<Texture2D>> _textures;
	UnorderedMap<String, UniquePtr<MeshRenderSet>> _meshes;

	ListArray<UniquePtr<IRenderableEntity>> _renderList;
};