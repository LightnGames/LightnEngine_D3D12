#pragma once

#include <Utility.h>

struct ID3D12Device;
struct BufferView;
struct IRenderableEntity;
struct GpuResourceDataPool;
struct VertexAndIndexBuffer;
class VertexShader;
class PixelShader;
class Texture2D;
class SharedMaterial;
class CommandContext;
class StaticSingleMeshRCG;

#include "RenderableEntity.h"

class GpuResourceManager :public Singleton<GpuResourceManager> {
public:
	GpuResourceManager();
	~GpuResourceManager();

	void createSharedMaterial(RefPtr<ID3D12Device> device, const SharedMaterialCreateSettings& settings);
	void createTextures(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& settings);
	void createVertexAndIndexBuffer(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& fileName);

	void loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial) const;
	void loadTexture(const String& textureName, RefPtr<Texture2D>& dstTexture) const;
	void loadVertexAndIndexBuffer(const String& meshName, RefPtr<VertexAndIndexBuffer>& dstBuffers) const;

	StaticSingleMeshRender createStaticSingleMeshRender(const String& name, const VectorArray<String>& materialNames) const;
	void removeStaticSingleMeshRender(RefPtr<StaticSingleMeshRCG> render);

	void shutdown();

	UnorderedMap<String, SharedMaterial>& getMaterials() const;
	const ListArray<StaticSingleMeshRCG>& getMeshes() const;

private:
	UniquePtr<GpuResourceDataPool> _resourcePool;
};