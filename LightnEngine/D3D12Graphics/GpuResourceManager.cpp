#include "GpuResourceManager.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"
#include "CommandContext.h"
#include "SharedMaterial.h"
#include "stdafx.h"
#include "GpuResourceDataPool.h"
#include "MeshRender.h"
#include "AABB.h"
#include <cassert>
#include <LMath.h>

GpuResourceManager* Singleton<GpuResourceManager>::_singleton = 0;

GpuResourceManager::GpuResourceManager() {
	_resourcePool = makeUnique<GpuResourceDataPool>();
}

GpuResourceManager::~GpuResourceManager() {
}

void GpuResourceManager::createSharedMaterial(RefPtr<ID3D12Device> device, const SharedMaterialCreateSettings& settings) {
	const String vertexShaderFullPath = "Shaders/" + settings.vertexShaderName;
	const String pixelShaderFullPath = "Shaders/" + settings.pixelShaderName;

	//頂点シェーダーキャッシュがあればそれを使う。なければ新規生成
	RefPtr<VertexShader> vertexShader = nullptr;
	if (_resourcePool->vertexShaders.count(vertexShaderFullPath) > 0) {
		loadVertexShader(vertexShaderFullPath, &vertexShader);
	}
	else {
		vertexShader = createVertexShader(vertexShaderFullPath, settings.inputLayouts);
	}

	//ピクセルシェーダーキャッシュがあればそれを使う。なければ新規生成
	RefPtr<PixelShader> pixelShader = nullptr;
	if (_resourcePool->pixelShaders.count(pixelShaderFullPath) > 0) {
		loadPixelShader(pixelShaderFullPath, &pixelShader);
	}
	else {
		pixelShader = createPixelShader(pixelShaderFullPath);
	}

	//ルートシグネチャキャッシュがあればそれを使う。なければ新規生成
	RefPtr<RootSignature> rootSignature = nullptr;
	if (_resourcePool->rootSignatures.count(settings.name) > 0) {
		rootSignature = &_resourcePool->rootSignatures.at(settings.name);
	}
	else {
		auto itr = _resourcePool->rootSignatures.emplace(std::piecewise_construct,
			std::make_tuple(settings.name),
			std::make_tuple());

		rootSignature = &(*itr.first).second;
		rootSignature->create(device, *vertexShader, *pixelShader);
	}

	//パイプラインステートキャッシュがあればそれを使う。なければ新規生成
	RefPtr<PipelineState> pipelineState = nullptr;
	if (_resourcePool->pipelineStates.count(settings.name) > 0) {
		pipelineState = &_resourcePool->pipelineStates.at(settings.name);
	}
	else {
		auto itr = _resourcePool->pipelineStates.emplace(std::piecewise_construct,
			std::make_tuple(settings.name),
			std::make_tuple());

		pipelineState = &(*itr.first).second;
		pipelineState->create(device, rootSignature, *vertexShader, *pixelShader, castTopologyToType(settings.topology));
	}

	assert(_resourcePool->sharedMaterials.count(settings.name) == 0 && "その名前のマテリアルはすでに存在します！");

	//生成したマテリアルをキャッシュに登録
	const ShaderReflectionResult& vsReflection = vertexShader->shaderReflectionResult;
	const ShaderReflectionResult& psReflection = pixelShader->shaderReflectionResult;
	auto itr = _resourcePool->sharedMaterials.emplace(std::piecewise_construct,
		std::make_tuple(settings.name),
		std::make_tuple(vsReflection,
			psReflection,
			pipelineState->getRefPipelineState(),
			rootSignature->getRefRootSignature(),
			settings.topology));

	SharedMaterial& material = (*itr.first).second;
	DescriptorHeapManager& manager = DescriptorHeapManager::instance();

	//頂点シェーダーテクスチャSRV生成
	if (vsReflection.srvRangeDescs.size() > 0) {
		VectorArray<RefPtr<ID3D12Resource>> textures(settings.vsTextures.size());

		for (size_t i = 0; i < settings.vsTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.vsTextures[i], &texturePtr);

			textures[i] = texturePtr->get();
		}
		//settings.vsTextures.size()
		manager.createTextureShaderResourceView(textures.data(), &material._srvVertex, static_cast<uint32>(settings.vsTextures.size()));
	}
	//assert(vertexShader->shaderReflectionResult.srvRangeDescs.size()== settings.vsTextures.size() && "頂点シェーダー定義と指定したテクスチャ枚数が異なります！");

	//ピクセルシェーダーテクスチャSRV生成
	if (psReflection.srvRangeDescs.size() > 0) {
		VectorArray<RefPtr<ID3D12Resource>> textures(settings.psTextures.size());

		for (size_t i = 0; i < settings.psTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.psTextures[i], &texturePtr);

			textures[i] = texturePtr->get();
		}

		manager.createTextureShaderResourceView(textures.data(), &material._srvPixel, static_cast<uint32>(settings.psTextures.size()));
	}
	//assert(pixelShader->shaderReflectionResult.srvRangeDescs.size() == settings.psTextures.size() && "ピクセルシェーダー定義と指定したテクスチャ枚数が異なります！");


	//頂点シェーダーの定数バッファサイズを取得して定数バッファ本体を生成
	VectorArray<uint32> vertexCbSizes = vertexShader->getConstantBufferSizes();
	if (!vertexCbSizes.empty()) {
		material._vertexConstantBuffer.create(device, vertexCbSizes);
	}

	//ピクセルシェーダーの定数バッファサイズを取得して定数バッファ本体を生成
	VectorArray<uint32> pixelCbSizes = pixelShader->getConstantBufferSizes();
	if (!pixelCbSizes.empty()) {
		material._pixelConstantBuffer.create(device, pixelCbSizes);
	}
}

//テクスチャをまとめて生成する。まとめて送るのでCPUオーバーヘッドが少ない
void GpuResourceManager::createTextures(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& settings) {	
	VectorArray<ComPtr<ID3D12Resource>> uploadHeaps(settings.size());
	auto commandListSet = commandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	for (size_t i = 0; i < settings.size(); ++i) {
		//すでにその名目のテクスチャが存在するなら生成はスキップ
		if (_resourcePool->textures.count(settings[i]) > 0) {
			continue;
		}

		const String fullPath = "Resources/" + settings[i];

		//テクスチャをキャッシュに生成
		auto itr = _resourcePool->textures.emplace(std::piecewise_construct,
			std::make_tuple(settings[i]),
			std::make_tuple());

		Texture2D& tex = (*itr.first).second;
		tex.createDeferred2(device, commandList, &uploadHeaps[i], fullPath);
	}

	//アップロードバッファをGPUオンリーバッファにコピー
	commandContext.executeCommandList(commandList);
	commandContext.discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	commandContext.waitForIdle();
}

struct RawVertex {
	Vector3 position;
	Vector3 normal;
	Vector3 tangent;
	Vector2 texcoord;

	bool operator==(const RawVertex& left) const {
		return position == left.position && texcoord == texcoord;
	}
};

#define HashCombine(hash,seed) hash + 0x9e3779b9 + (seed << 6) + (seed >> 2)

namespace std {
	template<>
	class hash<RawVertex> {
	public:

		size_t operator () (const RawVertex& p) const {
			size_t seed = 0;

			seed ^= HashCombine(hash<float>()(p.position.x), seed);
			seed ^= HashCombine(hash<float>()(p.position.y), seed);
			seed ^= HashCombine(hash<float>()(p.position.z), seed);
			seed ^= HashCombine(hash<float>()(p.normal.x), seed);
			seed ^= HashCombine(hash<float>()(p.normal.y), seed);
			seed ^= HashCombine(hash<float>()(p.normal.z), seed);
			seed ^= HashCombine(hash<float>()(p.texcoord.x), seed);
			seed ^= HashCombine(hash<float>()(p.texcoord.y), seed);
			return seed;
		}
	};
}

#include <fstream>
//#include <fbxsdk.h>
//using namespace fbxsdk;
void GpuResourceManager::createVertexAndIndexBuffer(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& fileNames) {
	VectorArray<ComPtr<ID3D12Resource>> uploadHeaps(fileNames.size() * 2);//読み込むファイル数×(頂点バッファ＋インデックスバッファ)
	auto commandListSet = commandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	uint32 uploadHeapCounter = 0;
	for (const auto& fileName : fileNames) {
		{
			//fbxsdk::FbxManager* manager = fbxsdk::FbxManager::Create();
			//FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
			//manager->SetIOSettings(ios);
			//FbxScene* scene = FbxScene::Create(manager, "");

			//FbxImporter* importer = FbxImporter::Create(manager, "");
			//String fullPath = "Resources/" + fileName;
			//bool isSuccsess = importer->Initialize(fullPath.c_str(), -1, manager->GetIOSettings());
			//assert(isSuccsess && "FBX読み込み失敗");

			//importer->Import(scene);
			//importer->Destroy();

			//FbxGeometryConverter geometryConverter(manager);
			//geometryConverter.Triangulate(scene, true);

			//FbxAxisSystem::DirectX.ConvertScene(scene);

			//FbxMesh* mesh = scene->GetMember<FbxMesh>(0);
			//const uint32 materialCount = scene->GetMaterialCount();
			//const uint32 vertexCount = mesh->GetControlPointsCount();
			//const uint32 polygonCount = mesh->GetPolygonCount();
			//const uint32 polygonVertexCount = 3;
			//const uint32 indexCount = polygonCount * polygonVertexCount;

			//FbxStringList uvSetNames;
			//bool bIsUnmapped = false;
			//mesh->GetUVSetNames(uvSetNames);

			//FbxLayerElementMaterial* meshMaterials = mesh->GetLayer(0)->GetMaterials();

			////マテリアルごとの頂点インデックス数を調べる
			//VectorArray<uint32> materialIndexSizes(materialCount);
			//for (uint32 i = 0; i < polygonCount; ++i) {
			//	const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
			//	materialIndexSizes[materialId] += polygonVertexCount;
			//}

			////マテリアルごとのインデックスオフセットを計算
			//VectorArray<uint32> materialIndexOffsets(materialCount);
			//for (size_t i = 0; i < materialIndexOffsets.size(); ++i) {
			//	for (size_t j = 0; j < i; ++j) {
			//		materialIndexOffsets[i] += materialIndexSizes[j];
			//	}
			//}

			//UnorderedMap<RawVertex, uint32> optimizedVertices;//重複しない頂点情報と新しい頂点インデックス
			//VectorArray<UINT32> indices(indexCount);//新しい頂点インデックスでできたインデックスバッファ
			//VectorArray<uint32> materialIndexCounter(materialCount);//マテリアルごとのインデックス数を管理

			//optimizedVertices.reserve(indexCount);

			//for (uint32 i = 0; i < polygonCount; ++i) {
			//	const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
			//	const uint32 materialIndexOffset = materialIndexOffsets[materialId];
			//	uint32& indexCount = materialIndexCounter[materialId];

			//	for (uint32 j = 0; j < polygonVertexCount; ++j) {
			//		const uint32 vertexIndex = mesh->GetPolygonVertex(i, j);
			//		FbxVector4 v = mesh->GetControlPointAt(vertexIndex);
			//		FbxVector4 normal;
			//		FbxVector2 texcoord;

			//		FbxString uvSetName = uvSetNames.GetStringAt(0);//UVSetは０番インデックスのみ対応
			//		mesh->GetPolygonVertexUV(i, j, uvSetName, texcoord, bIsUnmapped);
			//		mesh->GetPolygonVertexNormal(i, j, normal);

			//		RawVertex r;
			//		r.position = { (float)v[0], (float)v[1], -(float)v[2] };//FBXは右手座標系なので左手座標系に直すためにZを反転する
			//		r.normal = { (float)normal[0], (float)normal[1], -(float)normal[2] };
			//		r.texcoord = { (float)texcoord[0], 1 - (float)texcoord[1] };

			//		const Vector3 vectorUp = { 0.0f, 1, EPSILON };
			//		r.tangent = Vector3::cross(r.normal, vectorUp);

			//		//Zを反転するとポリゴンが左回りになるので右回りになるようにインデックスを0,1,2 → 2,1,0にする
			//		const uint32 indexInverseCorrectionedValue = indexCount + 2 - j;
			//		const uint32 indexPerMaterial = materialIndexOffset + indexInverseCorrectionedValue;
			//		if (optimizedVertices.count(r) == 0) {
			//			uint32 vertexIndex = static_cast<uint32>(optimizedVertices.size());
			//			indices[indexPerMaterial] = vertexIndex;
			//			optimizedVertices.emplace(r, vertexIndex);
			//		}
			//		else {
			//			indices[indexPerMaterial] = optimizedVertices.at(r);
			//		}

			//	}

			//	indexCount += polygonVertexCount;
			//}

			////UnorederedMapの配列からVectorArrayに変換
			//VectorArray<RawVertex> vertices(optimizedVertices.size());
			//for (const auto& vertex : optimizedVertices) {
			//	vertices[vertex.second] = vertex.first;
			//}

			//manager->Destroy();

			////マテリアルの描画範囲を設定
			//VectorArray<MaterialDrawRange> materialSlots;
			//materialSlots.reserve(materialCount);

			//for (size_t i = 0; i < materialCount; ++i) {
			//	materialSlots.emplace_back(materialIndexSizes[i], materialIndexOffsets[i]);
			//}
		}

		String fullPath = "Resources/" + fileName;
		std::ifstream fin(fullPath.c_str(), std::ios::in | std::ios::binary);
		fin.exceptions(std::ios::badbit);

		assert(!fin.fail() && "メッシュファイルが読み込めません");

		uint32 allFileSize = 0;
		uint32 verticesCount = 0;
		uint32 indicesCount = 0;
		uint32 materialCount = 0;

		fin.read(reinterpret_cast<char*>(&allFileSize), 4);
		fin.read(reinterpret_cast<char*>(&verticesCount), 4);
		fin.read(reinterpret_cast<char*>(&indicesCount), 4);
		fin.read(reinterpret_cast<char*>(&materialCount), 4);

		uint32 verticesSize = verticesCount * sizeof(RawVertex);
		uint32 indicesSize = indicesCount * sizeof(uint32);
		uint32 materialSize = materialCount * sizeof(MaterialDrawRange);

		VectorArray<RawVertex> vertices(verticesCount);
		VectorArray<uint32> indices(indicesCount);
		VectorArray<MaterialDrawRange> materialRanges(materialCount);
		AABB boundingBox;

		fin.read(reinterpret_cast<char*>(vertices.data()), verticesSize);
		fin.read(reinterpret_cast<char*>(indices.data()), indicesSize);
		fin.read(reinterpret_cast<char*>(materialRanges.data()), materialSize);
		fin.read(reinterpret_cast<char*>(&boundingBox), 24);

		fin.close();

		//メッシュ描画インスタンスを生成
		auto itr = _resourcePool->vertexAndIndexBuffers.emplace(std::piecewise_construct,
			std::make_tuple(fileName),
			std::make_tuple(materialRanges));

		VertexAndIndexBuffer& buffers = (*itr.first).second;
		buffers.boundingBox = boundingBox;

		//頂点バッファ生成
		buffers.vertexBuffer.createDeferred<RawVertex>(device, commandList, &uploadHeaps[uploadHeapCounter++], vertices);

		//インデックスバッファ
		buffers.indexBuffer.createDeferred(device, commandList, &uploadHeaps[uploadHeapCounter++], indices);
	}

	//アップロードバッファをGPUオンリーバッファにコピー
	commandContext.executeCommandList(commandList);
	commandContext.discardCommandListSet(commandListSet);

	//コピーが終わるまでアップロードヒープを破棄しない
	commandContext.waitForIdle();
}

void GpuResourceManager::createRootSignature(RefPtr<ID3D12Device> device, const String& name, const VectorArray<D3D12_ROOT_PARAMETER1>& rootParameters, RefPtr<D3D12_STATIC_SAMPLER_DESC> staticSampler){
	auto itr = _resourcePool->rootSignatures.emplace(std::piecewise_construct,
		std::make_tuple(name),
		std::make_tuple());

	RefPtr<RootSignature> rootSignature = &(*itr.first).second;
	rootSignature->create(device, rootParameters, staticSampler);
}

RefPtr<VertexShader> GpuResourceManager::createVertexShader(const String& fileName, const VectorArray<D3D12_INPUT_ELEMENT_DESC>& inputLayouts){
	auto itr = _resourcePool->vertexShaders.emplace(std::piecewise_construct,
		std::make_tuple(fileName),
		std::make_tuple());

	auto vertexShader = &(*itr.first).second;
	vertexShader->create(fileName, inputLayouts);

	return vertexShader;
}

RefPtr<PixelShader> GpuResourceManager::createPixelShader(const String& fileName){
	auto itr = _resourcePool->pixelShaders.emplace(std::piecewise_construct,
		std::make_tuple(fileName),
		std::make_tuple());

	auto pixelShader = &(*itr.first).second;
	pixelShader->create(fileName);

	return pixelShader;
}

void GpuResourceManager::loadSharedMaterial(const String & materialName, RefAddressOf<SharedMaterial> dstMaterial) const {
	assert(_resourcePool->sharedMaterials.count(materialName) > 0 && "マテリアルが見つかりません");
	*dstMaterial = &_resourcePool->sharedMaterials.at(materialName);
}

void GpuResourceManager::loadTexture(const String & textureName, RefAddressOf<Texture2D> dstTexture) const {
	assert(_resourcePool->textures.count(textureName) > 0 && "テクスチャが見つかりません");
	*dstTexture = &_resourcePool->textures.at(textureName);
}

void GpuResourceManager::loadVertexAndIndexBuffer(const String & meshName, RefAddressOf<VertexAndIndexBuffer> dstBuffers) const {
	assert(_resourcePool->vertexAndIndexBuffers.count(meshName) > 0 && "メッシュが見つかりません");
	*dstBuffers = &_resourcePool->vertexAndIndexBuffers.at(meshName);
}

void GpuResourceManager::loadVertexShader(const String & shaderName, RefAddressOf<VertexShader> dstShader) const {
	assert(_resourcePool->vertexShaders.count(shaderName) > 0 && "頂点シェーダーが見つかりません");
	*dstShader = &_resourcePool->vertexShaders.at(shaderName);
}

void GpuResourceManager::loadPixelShader(const String & shaderName, RefAddressOf<PixelShader> dstShader) const {
	assert(_resourcePool->pixelShaders.count(shaderName) > 0 && "ピクセルシェーダーが見つかりません");
	*dstShader = &_resourcePool->pixelShaders.at(shaderName);
}

void GpuResourceManager::shutdown() {
	_resourcePool.reset();
}

UnorderedMap<String, SharedMaterial>& GpuResourceManager::getMaterials() const {
	return _resourcePool->sharedMaterials;
}

RefPtr<Camera> GpuResourceManager::getMainCamera(){
	return &_mainCamera;
}
