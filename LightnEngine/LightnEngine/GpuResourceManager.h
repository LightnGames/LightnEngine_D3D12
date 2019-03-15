#pragma once

#include "Utility.h"
#include "stdafx.h"
#include <unordered_map> 

struct TextureInfo;
struct BufferView;
class VertexBuffer;
class IndexBuffer;
class RootSignature;
class PipelineState;
class ConstantBuffer;
class VertexShader;
class PixelShader;
class Texture2D;
class SharedMaterial;
class CommandContext;

USE_UNORDERED_MAP

struct SharedMaterialCreateSettings {
	String name;
	String vertexShaderName;
	String pixelShaderName;
	std::vector<String> vsTextures;
	std::vector<String> psTextures;
};

class GpuResourceManager :public Singleton<GpuResourceManager> {
public:
	~GpuResourceManager();

	void createSharedMaterial(ID3D12Device* device, const SharedMaterialCreateSettings& settings);
	void createTextures(ID3D12Device* device, CommandContext& commandContext, const std::vector<String>& settings);

	void loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial);
	void loadTexture(const String& textureName, RefPtr<Texture2D>& dstTexture);

	void shutdown();

private:
	UnorderedMap<String, UniquePtr<VertexShader>> _vertexShaders;
	UnorderedMap<String, UniquePtr<PixelShader>> _pixelShaders;
	UnorderedMap<String, UniquePtr<SharedMaterial>> _sharedMaterials;
	UnorderedMap<String, UniquePtr<Texture2D>> _textures;
};