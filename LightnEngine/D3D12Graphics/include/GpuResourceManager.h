#pragma once

#include <Utility.h>
#include <d3d12.h>

struct ID3D12Device;
struct BufferView;
struct IRenderableEntity;
struct GpuResourceDataPool;
struct VertexAndIndexBuffer;
struct SharedMaterialCreateSettings;
class VertexShader;
class PixelShader;
class Texture2D;
class SharedMaterial;
class CommandContext;
class StaticSingleMeshRCG;

class GpuResourceManager :public Singleton<GpuResourceManager> {
public:
	GpuResourceManager();
	~GpuResourceManager();

	void createSharedMaterial(RefPtr<ID3D12Device> device, const SharedMaterialCreateSettings& settings);
	void createTextures(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& settings);
	void createVertexAndIndexBuffer(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& fileName);
	
	RefPtr<VertexShader> createVertexShader(const String& fileName, const VectorArray<D3D12_INPUT_ELEMENT_DESC>& inputLayouts);
	RefPtr<PixelShader> createPixelShader(const String& fileName);

	void loadSharedMaterial(const String& materialName, RefAddressOf<SharedMaterial> dstMaterial) const;
	void loadTexture(const String& textureName, RefAddressOf<Texture2D> dstTexture) const;
	void loadVertexAndIndexBuffer(const String& meshName, RefAddressOf<VertexAndIndexBuffer> dstBuffers) const;
	void loadVertexShader(const String& shaderName, RefAddressOf<VertexShader> dstShader) const;
	void loadPixelShader(const String& shaderName, RefAddressOf<PixelShader> dstShader) const;

	void shutdown();

	UnorderedMap<String, SharedMaterial>& getMaterials() const;

private:
	UniquePtr<GpuResourceDataPool> _resourcePool;
};