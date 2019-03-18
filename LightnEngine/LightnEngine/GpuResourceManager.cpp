#include "GpuResourceManager.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"
#include "CommandContext.h"
#include "SharedMaterial.h"
#include "MeshRenderSet.h"
#include "stdafx.h"
#include <cassert>
#include <LMath.h>

GpuResourceManager* Singleton<GpuResourceManager>::_singleton = 0;

WString convertWString(const String& srcStr) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t, MyAllocator<wchar_t>> cv;

	//string��wstring UTF-8 Only No Include Japanese
	return cv.from_bytes(srcStr.c_str());
}

GpuResourceManager::~GpuResourceManager() {
	shutdown();
}

void GpuResourceManager::createSharedMaterial(ID3D12Device* device, const SharedMaterialCreateSettings& settings) {
	//���_�V�F�[�_�[�L���b�V��������΂�����g���B�Ȃ���ΐV�K����
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

	//�s�N�Z���V�F�[�_�[�L���b�V��������΂�����g���B�Ȃ���ΐV�K����
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

	//���_�V�F�[�_�[�e�N�X�`��SRV����
	if (settings.vsTextures.size() > 0) {
		VectorArray<RefPtr<ID3D12Resource>> textures(settings.vsTextures.size());

		for (size_t i = 0; i < settings.vsTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.vsTextures[i], texturePtr);

			textures[i] = texturePtr->get();
		}

		manager.createShaderResourceView(textures.data(), &material->srvVertex, static_cast<uint32>(settings.vsTextures.size()));
	}

	//�s�N�Z���V�F�[�_�[�e�N�X�`��SRV����
	if (settings.psTextures.size() > 0) {
		VectorArray<RefPtr<ID3D12Resource>> textures(settings.psTextures.size());

		for (size_t i = 0; i < settings.psTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.psTextures[i], texturePtr);

			textures[i] = texturePtr->get();
		}

		manager.createShaderResourceView(textures.data(), &material->srvPixel, static_cast<uint32>(settings.psTextures.size()));
	}

	//���_�V�F�[�_�[�̒萔�o�b�t�@�T�C�Y���擾���Ē萔�o�b�t�@�{�̂𐶐�
	VectorArray<uint32> vertexCbSizes = vertexShader->getConstantBufferSizes();
	if (!vertexCbSizes.empty()) {
		material->vertexConstantBuffer.create(device, vertexCbSizes);
	}

	//�s�N�Z���V�F�[�_�[�̒萔�o�b�t�@�T�C�Y���擾���Ē萔�o�b�t�@�{�̂𐶐�
	VectorArray<uint32> pixelCbSizes = pixelShader->getConstantBufferSizes();
	if (!pixelCbSizes.empty()) {
		material->pixelConstantBuffer.create(device, pixelCbSizes);
	}

	//���������}�e���A�����L���b�V���ɓo�^
	_sharedMaterials.emplace(settings.name, material);
}

//�e�N�X�`�����܂Ƃ߂Đ�������B�܂Ƃ߂đ���̂�CPU�I�[�o�[�w�b�h�����Ȃ�
void GpuResourceManager::createTextures(ID3D12Device* device, CommandContext& commandContext, const VectorArray<String>& settings) {
	VectorArray<ComPtr<ID3D12Resource>> uploadHeaps(settings.size());
	auto commandListSet = commandContext.requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	for (size_t i = 0; i < settings.size(); ++i) {
		Texture2D* texture = new Texture2D();
		texture->createDeferred2(device, commandList, &uploadHeaps[i], settings[i]);

		_textures.emplace(settings[i], texture);
	}

	//�A�b�v���[�h�o�b�t�@��GPU�I�����[�o�b�t�@�ɃR�s�[
	commandContext.executeCommandList(commandList);
	commandContext.discardCommandListSet(commandListSet);

	//�R�s�[���I���܂ŃA�b�v���[�h�q�[�v��j�����Ȃ�
	commandContext.waitForIdle();
}

struct RawVertex {
	Vector3 position;
	//Vector3 normal;
	Vector2 texcoord;

	bool operator==(const RawVertex &left) const {
		return position == left.position && texcoord == texcoord;
	}
};

#define HashCombine(hash,seed) hash + 0x9e3779b9 + (seed << 6) + (seed >> 2)

namespace std {
	template<>
	class hash<RawVertex> {
	public:

		size_t operator () (const RawVertex &p) const {
			size_t seed = 0;

			seed ^= HashCombine(hash<float>()(p.position.x), seed);
			seed ^= HashCombine(hash<float>()(p.position.y), seed);
			seed ^= HashCombine(hash<float>()(p.position.z), seed);
			seed ^= HashCombine(hash<float>()(p.texcoord.x), seed);
			seed ^= HashCombine(hash<float>()(p.texcoord.y), seed);
			return seed;
		}
	};
}

#include <unordered_map>
USE_UNORDERED_MAP

#include <fbxsdk.h>
using namespace fbxsdk;
void GpuResourceManager::createMeshSets(ID3D12Device * device, CommandContext & commandContext, const String & fileName) {
	fbxsdk::FbxManager* manager = fbxsdk::FbxManager::Create();
	FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ios);
	FbxScene* scene = FbxScene::Create(manager, "");

	FbxImporter* importer = FbxImporter::Create(manager, "");
	importer->Initialize(fileName.c_str(), -1, manager->GetIOSettings());
	importer->Import(scene);
	importer->Destroy();

	FbxGeometryConverter geometryConverter(manager);
	geometryConverter.Triangulate(scene, true);

	FbxMesh* mesh = scene->GetMember<FbxMesh>(0);
	const uint32 materialCount = mesh->GetElementMaterialCount();
	const uint32 vertexCount = mesh->GetControlPointsCount();
	const uint32 polygonCount = mesh->GetPolygonCount();
	const uint32 polygonVertexCount = 3;
	const uint32 indexCount = polygonCount * polygonVertexCount;

	FbxStringList uvSetNames;
	bool bIsUnmapped = false;
	mesh->GetUVSetNames(uvSetNames);

	FbxLayerElementMaterial* meshMaterials = mesh->GetLayer(0)->GetMaterials();

	//�}�e���A�����Ƃ̒��_�C���f�b�N�X���𒲂ׂ�
	VectorArray<uint32> materialIndexSize(materialCount);
	for (int i = 0; i < polygonCount; ++i) {
		const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
		materialIndexSize[materialId] += polygonVertexCount;
	}

	UnorderedMap<RawVertex, uint32> optimizedVertices;//�d�����Ȃ����_���ƐV�������_�C���f�b�N�X
	VectorArray<UINT32> indices(indexCount);//�V�������_�C���f�b�N�X�łł����C���f�b�N�X�o�b�t�@
	VectorArray<uint32> materialIndexCounter(materialCount);//�}�e���A�����Ƃ̃C���f�b�N�X�����Ǘ�

	optimizedVertices.reserve(indexCount);

	for (uint32 i = 0; i < polygonCount; ++i) {
		const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
		uint32& checkIndex = materialIndexCounter[materialId];

		for (uint32 j = 0; j < polygonVertexCount; ++j) {
			const uint32 vertexIndex = mesh->GetPolygonVertex(i, j);
			FbxVector4 v = mesh->GetControlPointAt(vertexIndex);
			FbxVector4 normal;
			FbxVector2 texcoord;

			FbxString uvSetName = uvSetNames.GetStringAt(0);//UVSet�͂O�ԃC���f�b�N�X�̂ݑΉ�
			mesh->GetPolygonVertexUV(i, j, uvSetName, texcoord, bIsUnmapped);
			mesh->GetPolygonVertexNormal(i, j, normal);

			RawVertex r;
			r.position = { (float)v[0], (float)v[1], (float)v[2] };
			//r.normal = { (float)normal[0], (float)normal[1], (float)normal[2] };
			r.texcoord = { (float)texcoord[0], (float)texcoord[1] };

			if (optimizedVertices.count(r) == 0) {
				uint32 vertexIndex = static_cast<uint32>(optimizedVertices.size());
				indices[checkIndex] = vertexIndex;
				optimizedVertices.emplace(r, vertexIndex);
			}
			else {
				indices[checkIndex] = optimizedVertices.at(r);
			}

			checkIndex++;
		}
	}

	//UnorederedMap�̔z�񂩂�VectorArray�ɕϊ�
	VectorArray<RawVertex> vertices(optimizedVertices.size());
	for (const auto& vertex : optimizedVertices) {
		vertices[vertex.second] = vertex.first;
	}

	manager->Destroy();

	MeshRenderSet* meshSet = new MeshRenderSet();
	ComPtr<ID3D12Resource> uploadHeaps[2] = {};
	auto commandListSet = commandContext.requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;

	//���_�o�b�t�@����
	meshSet->_vertexBuffer = makeUnique<VertexBuffer>();
	meshSet->_vertexBuffer->createDeferred<RawVertex>(device, commandList, &uploadHeaps[0], vertices);

	//�C���f�b�N�X�o�b�t�@
	meshSet->_indexBuffer = makeUnique<IndexBuffer>();
	meshSet->_indexBuffer->createDeferred(device, commandList, &uploadHeaps[1], indices);

	//�A�b�v���[�h�o�b�t�@��GPU�I�����[�o�b�t�@�ɃR�s�[
	commandContext.executeCommandList(commandList);
	commandContext.discardCommandListSet(commandListSet);

	//�R�s�[���I���܂ŃA�b�v���[�h�q�[�v��j�����Ȃ�
	commandContext.waitForIdle();

	MaterialSlot slot;
	slot.indexCount = static_cast<uint32>(indices.size());
	slot.indexOffset = 0;
	loadSharedMaterial("TestM", slot.material);

	meshSet->_materialSlots.emplace_back(slot);
	_meshes.emplace(fileName, std::move(meshSet));
}

void GpuResourceManager::loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial) {
	assert(_sharedMaterials.count(materialName) > 0 && "�}�e���A����������܂���");
	dstMaterial = _sharedMaterials.at(materialName).get();
}

void GpuResourceManager::loadTexture(const String & textureName, RefPtr<Texture2D>& dstTexture) {
	assert(_textures.count(textureName) > 0 && "�e�N�X�`����������܂���");
	dstTexture = _textures.at(textureName).get();
}

void GpuResourceManager::loadMeshSets(const String & meshName, RefPtr<MeshRenderSet>& dstMeshSet) {
	assert(_meshes.count(meshName) > 0 && "���b�V����������܂���");
	dstMeshSet = _meshes.at(meshName).get();
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
