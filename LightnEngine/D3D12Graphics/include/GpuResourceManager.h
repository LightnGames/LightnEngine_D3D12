#pragma once

#include <Utility.h>
#include <d3d12.h>

#include "Camera.h"

struct ID3D12Device;
struct BufferView;
struct IRenderableEntity;
struct GpuResourceDataPool;
struct VertexAndIndexBuffer;
struct SharedMaterialCreateSettings;
struct DefaultPipelineStateDescSet;
class GpuBuffer;
class PipelineState;
class RootSignature;
class ConstantBuffer;
class VertexShader;
class PixelShader;
class Texture2D;
class SingleMeshRenderMaterial;
class CommandContext;
class StaticSingleMeshRCG;

class GpuResourceManager :public Singleton<GpuResourceManager> {
public:
	GpuResourceManager();
	~GpuResourceManager();

	void createSharedMaterial(RefPtr<ID3D12Device> device, const SharedMaterialCreateSettings& settings);
	void createTextures(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& settings);
	void createVertexAndIndexBuffer(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& fileName);
	RefPtr<ConstantBuffer> createConstantBuffer(RefPtr<ID3D12Device> device, const String& name, uint32 size);

	RefPtr<PipelineState> createComputePipelineState(RefPtr<ID3D12Device> device, const String& name, const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc);
	RefPtr<PipelineState> createPipelineState(RefPtr<ID3D12Device> device, const String& name, RefPtr<RootSignature> rootSignature,
		const RefPtr<VertexShader> vertexShader, const RefPtr<PixelShader> pixelShader,
		const DefaultPipelineStateDescSet& psoDescSet);

	RefPtr<GpuBuffer> createOnlyGpuBuffer(const String& name);
	RefPtr<RootSignature> createRootSignature(RefPtr<ID3D12Device> device, const String& name, const VectorArray<D3D12_ROOT_PARAMETER1>& rootParameters, RefPtr<D3D12_STATIC_SAMPLER_DESC> staticSampler = nullptr);
	RefPtr<BufferView> createTextureBufferView(const String& viewName, const VectorArray<String>& textureNames);
	RefPtr<BufferView> createOnlyBufferView(const String& viewName);
	RefPtr<VertexShader> createVertexShader(const String& fileName, const VectorArray<D3D12_INPUT_ELEMENT_DESC>& inputLayouts);
	RefPtr<PixelShader> createPixelShader(const String& fileName);

	//void loadSharedMaterial(const String& materialName, RefAddressOf<SingleMeshRenderPass> dstMaterial) const;
	void loadConstantBuffer(const String& constantBufferName, RefAddressOf<ConstantBuffer> dstBuffer) const;
	void loadTexture(const String& textureName, RefAddressOf<Texture2D> dstTexture) const;
	void loadVertexAndIndexBuffer(const String& meshName, RefAddressOf<VertexAndIndexBuffer> dstBuffers) const;
	void loadVertexShader(const String& shaderName, RefAddressOf<VertexShader> dstShader) const;
	void loadPixelShader(const String& shaderName, RefAddressOf<PixelShader> dstShader) const;

	void shutdown();

	//UnorderedMap<String, SingleMeshRenderPass>& getMaterials() const;
	RefPtr<Camera> getMainCamera();

private:
	UniquePtr<GpuResourceDataPool> _resourcePool;
	Camera _mainCamera;
};