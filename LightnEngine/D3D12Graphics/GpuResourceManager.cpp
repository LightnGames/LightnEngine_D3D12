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

	//���_�V�F�[�_�[�L���b�V��������΂�����g���B�Ȃ���ΐV�K����
	RefPtr<VertexShader> vertexShader = nullptr;
	if (_resourcePool->vertexShaders.count(vertexShaderFullPath) > 0) {
		loadVertexShader(vertexShaderFullPath, &vertexShader);
	}
	else {
		vertexShader = createVertexShader(vertexShaderFullPath, settings.inputLayouts);
	}

	//�s�N�Z���V�F�[�_�[�L���b�V��������΂�����g���B�Ȃ���ΐV�K����
	RefPtr<PixelShader> pixelShader = nullptr;
	if (_resourcePool->pixelShaders.count(pixelShaderFullPath) > 0) {
		loadPixelShader(pixelShaderFullPath, &pixelShader);
	}
	else {
		pixelShader = createPixelShader(pixelShaderFullPath);
	}

	//���[�g�V�O�l�`���L���b�V��������΂�����g���B�Ȃ���ΐV�K����
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

	//�p�C�v���C���X�e�[�g�L���b�V��������΂�����g���B�Ȃ���ΐV�K����
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

	assert(_resourcePool->sharedMaterials.count(settings.name) == 0 && "���̖��O�̃}�e���A���͂��łɑ��݂��܂��I");

	//���������}�e���A�����L���b�V���ɓo�^
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

	//���_�V�F�[�_�[�e�N�X�`��SRV����
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
	//assert(vertexShader->shaderReflectionResult.srvRangeDescs.size()== settings.vsTextures.size() && "���_�V�F�[�_�[��`�Ǝw�肵���e�N�X�`���������قȂ�܂��I");

	//�s�N�Z���V�F�[�_�[�e�N�X�`��SRV����
	if (psReflection.srvRangeDescs.size() > 0) {
		VectorArray<RefPtr<ID3D12Resource>> textures(settings.psTextures.size());

		for (size_t i = 0; i < settings.psTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.psTextures[i], &texturePtr);

			textures[i] = texturePtr->get();
		}

		manager.createTextureShaderResourceView(textures.data(), &material._srvPixel, static_cast<uint32>(settings.psTextures.size()));
	}
	//assert(pixelShader->shaderReflectionResult.srvRangeDescs.size() == settings.psTextures.size() && "�s�N�Z���V�F�[�_�[��`�Ǝw�肵���e�N�X�`���������قȂ�܂��I");


	//���_�V�F�[�_�[�̒萔�o�b�t�@�T�C�Y���擾���Ē萔�o�b�t�@�{�̂𐶐�
	VectorArray<uint32> vertexCbSizes = vertexShader->getConstantBufferSizes();
	if (!vertexCbSizes.empty()) {
		material._vertexConstantBuffer.create(device, vertexCbSizes);
	}

	//�s�N�Z���V�F�[�_�[�̒萔�o�b�t�@�T�C�Y���擾���Ē萔�o�b�t�@�{�̂𐶐�
	VectorArray<uint32> pixelCbSizes = pixelShader->getConstantBufferSizes();
	if (!pixelCbSizes.empty()) {
		material._pixelConstantBuffer.create(device, pixelCbSizes);
	}
}

//�e�N�X�`�����܂Ƃ߂Đ�������B�܂Ƃ߂đ���̂�CPU�I�[�o�[�w�b�h�����Ȃ�
void GpuResourceManager::createTextures(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& settings) {	
	VectorArray<ComPtr<ID3D12Resource>> uploadHeaps(settings.size());
	auto commandListSet = commandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	for (size_t i = 0; i < settings.size(); ++i) {
		//���łɂ��̖��ڂ̃e�N�X�`�������݂���Ȃ琶���̓X�L�b�v
		if (_resourcePool->textures.count(settings[i]) > 0) {
			continue;
		}

		const String fullPath = "Resources/" + settings[i];

		//�e�N�X�`�����L���b�V���ɐ���
		auto itr = _resourcePool->textures.emplace(std::piecewise_construct,
			std::make_tuple(settings[i]),
			std::make_tuple());

		Texture2D& tex = (*itr.first).second;
		tex.createDeferred2(device, commandList, &uploadHeaps[i], fullPath);
	}

	//�A�b�v���[�h�o�b�t�@��GPU�I�����[�o�b�t�@�ɃR�s�[
	commandContext.executeCommandList(commandList);
	commandContext.discardCommandListSet(commandListSet);

	//�R�s�[���I���܂ŃA�b�v���[�h�q�[�v��j�����Ȃ�
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
	VectorArray<ComPtr<ID3D12Resource>> uploadHeaps(fileNames.size() * 2);//�ǂݍ��ރt�@�C�����~(���_�o�b�t�@�{�C���f�b�N�X�o�b�t�@)
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
			//assert(isSuccsess && "FBX�ǂݍ��ݎ��s");

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

			////�}�e���A�����Ƃ̒��_�C���f�b�N�X���𒲂ׂ�
			//VectorArray<uint32> materialIndexSizes(materialCount);
			//for (uint32 i = 0; i < polygonCount; ++i) {
			//	const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
			//	materialIndexSizes[materialId] += polygonVertexCount;
			//}

			////�}�e���A�����Ƃ̃C���f�b�N�X�I�t�Z�b�g���v�Z
			//VectorArray<uint32> materialIndexOffsets(materialCount);
			//for (size_t i = 0; i < materialIndexOffsets.size(); ++i) {
			//	for (size_t j = 0; j < i; ++j) {
			//		materialIndexOffsets[i] += materialIndexSizes[j];
			//	}
			//}

			//UnorderedMap<RawVertex, uint32> optimizedVertices;//�d�����Ȃ����_���ƐV�������_�C���f�b�N�X
			//VectorArray<UINT32> indices(indexCount);//�V�������_�C���f�b�N�X�łł����C���f�b�N�X�o�b�t�@
			//VectorArray<uint32> materialIndexCounter(materialCount);//�}�e���A�����Ƃ̃C���f�b�N�X�����Ǘ�

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

			//		FbxString uvSetName = uvSetNames.GetStringAt(0);//UVSet�͂O�ԃC���f�b�N�X�̂ݑΉ�
			//		mesh->GetPolygonVertexUV(i, j, uvSetName, texcoord, bIsUnmapped);
			//		mesh->GetPolygonVertexNormal(i, j, normal);

			//		RawVertex r;
			//		r.position = { (float)v[0], (float)v[1], -(float)v[2] };//FBX�͉E����W�n�Ȃ̂ō�����W�n�ɒ������߂�Z�𔽓]����
			//		r.normal = { (float)normal[0], (float)normal[1], -(float)normal[2] };
			//		r.texcoord = { (float)texcoord[0], 1 - (float)texcoord[1] };

			//		const Vector3 vectorUp = { 0.0f, 1, EPSILON };
			//		r.tangent = Vector3::cross(r.normal, vectorUp);

			//		//Z�𔽓]����ƃ|���S���������ɂȂ�̂ŉE���ɂȂ�悤�ɃC���f�b�N�X��0,1,2 �� 2,1,0�ɂ���
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

			////UnorederedMap�̔z�񂩂�VectorArray�ɕϊ�
			//VectorArray<RawVertex> vertices(optimizedVertices.size());
			//for (const auto& vertex : optimizedVertices) {
			//	vertices[vertex.second] = vertex.first;
			//}

			//manager->Destroy();

			////�}�e���A���̕`��͈͂�ݒ�
			//VectorArray<MaterialDrawRange> materialSlots;
			//materialSlots.reserve(materialCount);

			//for (size_t i = 0; i < materialCount; ++i) {
			//	materialSlots.emplace_back(materialIndexSizes[i], materialIndexOffsets[i]);
			//}
		}

		String fullPath = "Resources/" + fileName;
		std::ifstream fin(fullPath.c_str(), std::ios::in | std::ios::binary);
		fin.exceptions(std::ios::badbit);

		assert(!fin.fail() && "���b�V���t�@�C�����ǂݍ��߂܂���");

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

		//���b�V���`��C���X�^���X�𐶐�
		auto itr = _resourcePool->vertexAndIndexBuffers.emplace(std::piecewise_construct,
			std::make_tuple(fileName),
			std::make_tuple(materialRanges));

		VertexAndIndexBuffer& buffers = (*itr.first).second;
		buffers.boundingBox = boundingBox;

		//���_�o�b�t�@����
		buffers.vertexBuffer.createDeferred<RawVertex>(device, commandList, &uploadHeaps[uploadHeapCounter++], vertices);

		//�C���f�b�N�X�o�b�t�@
		buffers.indexBuffer.createDeferred(device, commandList, &uploadHeaps[uploadHeapCounter++], indices);
	}

	//�A�b�v���[�h�o�b�t�@��GPU�I�����[�o�b�t�@�ɃR�s�[
	commandContext.executeCommandList(commandList);
	commandContext.discardCommandListSet(commandListSet);

	//�R�s�[���I���܂ŃA�b�v���[�h�q�[�v��j�����Ȃ�
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
	assert(_resourcePool->sharedMaterials.count(materialName) > 0 && "�}�e���A����������܂���");
	*dstMaterial = &_resourcePool->sharedMaterials.at(materialName);
}

void GpuResourceManager::loadTexture(const String & textureName, RefAddressOf<Texture2D> dstTexture) const {
	assert(_resourcePool->textures.count(textureName) > 0 && "�e�N�X�`����������܂���");
	*dstTexture = &_resourcePool->textures.at(textureName);
}

void GpuResourceManager::loadVertexAndIndexBuffer(const String & meshName, RefAddressOf<VertexAndIndexBuffer> dstBuffers) const {
	assert(_resourcePool->vertexAndIndexBuffers.count(meshName) > 0 && "���b�V����������܂���");
	*dstBuffers = &_resourcePool->vertexAndIndexBuffers.at(meshName);
}

void GpuResourceManager::loadVertexShader(const String & shaderName, RefAddressOf<VertexShader> dstShader) const {
	assert(_resourcePool->vertexShaders.count(shaderName) > 0 && "���_�V�F�[�_�[��������܂���");
	*dstShader = &_resourcePool->vertexShaders.at(shaderName);
}

void GpuResourceManager::loadPixelShader(const String & shaderName, RefAddressOf<PixelShader> dstShader) const {
	assert(_resourcePool->pixelShaders.count(shaderName) > 0 && "�s�N�Z���V�F�[�_�[��������܂���");
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
