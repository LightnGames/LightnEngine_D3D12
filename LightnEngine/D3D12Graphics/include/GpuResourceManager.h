#pragma once

#include <Utility.h>

struct ID3D12Device;
struct BufferView;
struct IRenderableEntity;
struct GpuResourceDataPool;
class MeshRenderSet;
class VertexShader;
class PixelShader;
class Texture2D;
class SharedMaterial;
class CommandContext;
class StaticSingleMeshRender;

class GpuResourceManager :public Singleton<GpuResourceManager> {
public:
	GpuResourceManager();
	~GpuResourceManager();

	void createSharedMaterial(RefPtr<ID3D12Device> device, const SharedMaterialCreateSettings& settings);
	void createTextures(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& settings);
	void createMeshSets(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& fileName);

	void loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial) const;
	void loadTexture(const String& textureName, RefPtr<Texture2D>& dstTexture) const;
	void loadMeshSets(const String& meshName, RefPtr<MeshRenderSet>& dstMeshSet) const;

	RefPtr<StaticSingleMeshRender> createStaticSingleMeshRender(const String& name, const VectorArray<String>& materialNames) const;
	void removeStaticSingleMeshRender(RefPtr<StaticSingleMeshRender> render);

	void shutdown();

	const ListArray<StaticSingleMeshRender>& getMeshes() const;

private:
	UniquePtr<GpuResourceDataPool> _resourcePool;
};