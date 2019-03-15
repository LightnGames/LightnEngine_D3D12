#include "GpuResourceManager.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"
#include "CommandContext.h"
#include "SharedMaterial.h"
#include <cassert>

GpuResourceManager* Singleton<GpuResourceManager>::_singleton = 0;
uint32 SharedMaterial::frameIndex = 0;

WString convertWString(const String& srcStr) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t, MyAllocator<wchar_t>> cv;

	//string→wstring UTF-8 Only No Include Japanese
	return cv.from_bytes(srcStr.c_str());
}

GpuResourceManager::~GpuResourceManager() {
	shutdown();
}

void GpuResourceManager::createSharedMaterial(ID3D12Device* device, const SharedMaterialCreateSettings& settings) {
	//頂点シェーダーキャッシュがあればそれを使う。なければ新規生成
	RefPtr<VertexShader> vertexShader = nullptr;
	if (_vertexShaders.count(settings.vertexShaderName) > 0) {
		vertexShader = _vertexShaders.at(settings.vertexShaderName).get();
	}
	else {
		UniquePtr<VertexShader> newVertexShader = makeUnique<VertexShader>();
		newVertexShader->create(settings.vertexShaderName);
		vertexShader = newVertexShader.get();
		_vertexShaders.emplace(settings.vertexShaderName, std::move(newVertexShader));
	}

	//ピクセルシェーダーキャッシュがあればそれを使う。なければ新規生成
	RefPtr<PixelShader> pixelShader = nullptr;
	if (_pixelShaders.count(settings.pixelShaderName) > 0) {
		pixelShader = _pixelShaders.at(settings.pixelShaderName).get();
	}
	else {
		UniquePtr<PixelShader> newPixelShader = makeUnique<PixelShader>();
		newPixelShader->create(settings.pixelShaderName);
		pixelShader = newPixelShader.get();
		_pixelShaders.emplace(settings.pixelShaderName, std::move(newPixelShader));
	}

	SharedMaterial* material = new SharedMaterial();
	material->create(device, vertexShader, pixelShader);

	DescriptorHeapManager& manager = DescriptorHeapManager::instance();
	
	//頂点シェーダーテクスチャSRV生成
	if (settings.vsTextures.size() > 0) {
		RefPtr<ID3D12Resource>* textures = new RefPtr<ID3D12Resource>[settings.vsTextures.size()];

		for (size_t i = 0; i < settings.vsTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.vsTextures[i], texturePtr);

			textures[i] = texturePtr->get();
		}

		manager.createShaderResourceView(textures, &material->srvVertex, static_cast<uint32>(settings.vsTextures.size()));
		delete[] textures;
	}

	//ピクセルシェーダーテクスチャSRV生成
	if (settings.psTextures.size() > 0) {
		RefPtr<ID3D12Resource>* textures = new RefPtr<ID3D12Resource>[settings.psTextures.size()];

		for (size_t i = 0; i < settings.psTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.psTextures[i], texturePtr);

			textures[i] = texturePtr->get();
		}

		manager.createShaderResourceView(textures, &material->srvPixel, static_cast<uint32>(settings.psTextures.size()));
		delete[] textures;
	}

	VectorArray<uint32> vertexCbSizes(vertexShader->result.constantBuffers.size());
	for (size_t i = 0; i < vertexCbSizes.size(); ++i) {
		vertexCbSizes[i] = vertexShader->result.constantBuffers[i].getBufferSize();
	}

	material->vertexConstantBuffer.create(device, vertexCbSizes);

	_sharedMaterials.emplace(settings.name, material);
}

//テクスチャをまとめて生成する。まとめて送るのでCPUオーバーヘッドが少ない
void GpuResourceManager::createTextures(ID3D12Device* device, CommandContext& commandContext, const std::vector<String>& settings) {
	VectorArray<ComPtr<ID3D12Resource>> uploadHeaps(settings.size());
	auto commandListSet = commandContext.requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	for (size_t i = 0; i < settings.size(); ++i) {
		Texture2D* texture = new Texture2D();
		texture->createDeferred2(device, commandList, &uploadHeaps[i], settings[i]);

		_textures.emplace(settings[i], texture);
	}

	//アップロードバッファをGPUオンリーバッファにコピー
	commandContext.executeCommandList(commandList);
	commandContext.discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	commandContext.waitForIdle();
}

void GpuResourceManager::loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial) {
	assert(_sharedMaterials.count(materialName) > 0 && "マテリアルが見つかりません");
	dstMaterial = _sharedMaterials.at(materialName).get();
}

void GpuResourceManager::loadTexture(const String & textureName, RefPtr<Texture2D>& dstTexture) {
	assert(_textures.count(textureName) > 0 && "テクスチャが見つかりません");
	dstTexture = _textures.at(textureName).get();
}

void GpuResourceManager::shutdown() {
	for (auto&& material : _sharedMaterials) {
		material.second.reset();
	}

	for (auto&& texture : _textures) {
		texture.second.reset();
	}

	_sharedMaterials.clear();
	_textures.clear();
}
