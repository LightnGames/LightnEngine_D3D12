#include "GpuResourceManager.h"
#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"
#include "CommandContext.h"
#include "SharedMaterial.h"
#include "MeshRenderSet.h"
#include "stdafx.h"
#include "GpuResourceDataPool.h"
#include <RenderableEntity.h>
#include <cassert>
#include <LMath.h>

GpuResourceManager* Singleton<GpuResourceManager>::_singleton = 0;

GpuResourceManager::GpuResourceManager(){
	_resourcePool = makeUnique<GpuResourceDataPool>();
}

GpuResourceManager::~GpuResourceManager() {
}

void GpuResourceManager::createSharedMaterial(RefPtr<ID3D12Device> device, const SharedMaterialCreateSettings& settings) {
	//���_�V�F�[�_�[�L���b�V��������΂�����g���B�Ȃ���ΐV�K����
	RefPtr<VertexShader> vertexShader = nullptr;
	if (_resourcePool->_vertexShaders.count(settings.vertexShaderName) > 0) {
		vertexShader = &(_resourcePool->_vertexShaders.at(settings.vertexShaderName));
	}
	else {
		VertexShader newVertexShader;
		String fullPath = "Shaders/" + settings.vertexShaderName;
		newVertexShader.create(fullPath);
		_resourcePool->_vertexShaders.emplace(settings.vertexShaderName, newVertexShader);
		vertexShader = &(_resourcePool->_vertexShaders.at(settings.vertexShaderName));
	}

	//�s�N�Z���V�F�[�_�[�L���b�V��������΂�����g���B�Ȃ���ΐV�K����
	RefPtr<PixelShader> pixelShader = nullptr;
	if (_resourcePool->_pixelShaders.count(settings.pixelShaderName) > 0) {
		pixelShader = &(_resourcePool->_pixelShaders.at(settings.pixelShaderName));
	}
	else {
		PixelShader newPixelShader;
		String fullPath = "Shaders/" + settings.pixelShaderName;
		newPixelShader.create(fullPath);
		_resourcePool->_pixelShaders.emplace(settings.pixelShaderName, newPixelShader);
		pixelShader = &(_resourcePool->_pixelShaders.at(settings.pixelShaderName));
	}

	//���������}�e���A�����L���b�V���ɓo�^
	auto itr = _resourcePool->_sharedMaterials.emplace(std::piecewise_construct,
		std::make_tuple(settings.name),
		std::make_tuple());

	SharedMaterial& material = (*itr.first).second;
	material.create(device, vertexShader, pixelShader);

	DescriptorHeapManager& manager = DescriptorHeapManager::instance();

	//���_�V�F�[�_�[�e�N�X�`��SRV����
	if (vertexShader->shaderReflectionResult.srvRangeDescs.size() > 0) {
		VectorArray<RefPtr<ID3D12Resource>> textures(settings.vsTextures.size());

		for (size_t i = 0; i < settings.vsTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.vsTextures[i], texturePtr);

			textures[i] = texturePtr->get();
		}
		//settings.vsTextures.size()
		manager.createShaderResourceView(textures.data(), &material.srvVertex, static_cast<uint32>(settings.vsTextures.size()));
	}
	//assert(vertexShader->shaderReflectionResult.srvRangeDescs.size()== settings.vsTextures.size() && "���_�V�F�[�_�[��`�Ǝw�肵���e�N�X�`���������قȂ�܂��I");

	//�s�N�Z���V�F�[�_�[�e�N�X�`��SRV����
	if (pixelShader->shaderReflectionResult.srvRangeDescs.size() > 0) {
		VectorArray<RefPtr<ID3D12Resource>> textures(settings.psTextures.size());

		for (size_t i = 0; i < settings.psTextures.size(); ++i) {
			RefPtr<Texture2D> texturePtr;
			loadTexture(settings.psTextures[i], texturePtr);

			textures[i] = texturePtr->get();
		}

		manager.createShaderResourceView(textures.data(), &material.srvPixel, static_cast<uint32>(settings.psTextures.size()));
	}
	//assert(pixelShader->shaderReflectionResult.srvRangeDescs.size() == settings.psTextures.size() && "�s�N�Z���V�F�[�_�[��`�Ǝw�肵���e�N�X�`���������قȂ�܂��I");


	//���_�V�F�[�_�[�̒萔�o�b�t�@�T�C�Y���擾���Ē萔�o�b�t�@�{�̂𐶐�
	VectorArray<uint32> vertexCbSizes = vertexShader->getConstantBufferSizes();
	if (!vertexCbSizes.empty()) {
		material.vertexConstantBuffer.create(device, vertexCbSizes);
	}

	//���_�V�F�[�_�[�̃��[�g32bit�萔�T�C�Y���擾���ď�����
	VectorArray<uint32> vertexRoot32bitSizes = vertexShader->getRoot32bitConstantSizes();
	if (!vertexRoot32bitSizes.empty()) {
		material.vertexRoot32bitConstant.create(device, vertexRoot32bitSizes);
	}

	//�s�N�Z���V�F�[�_�[�̒萔�o�b�t�@�T�C�Y���擾���Ē萔�o�b�t�@�{�̂𐶐�
	VectorArray<uint32> pixelCbSizes = pixelShader->getConstantBufferSizes();
	if (!pixelCbSizes.empty()) {
		material.pixelConstantBuffer.create(device, pixelCbSizes);
	}

	//�s�N�Z���V�F�[�_�[�̃��[�g32bit�萔�T�C�Y���擾���ď�����
	VectorArray<uint32> pixelRoot32bitSizes = pixelShader->getRoot32bitConstantSizes();
	if (!pixelRoot32bitSizes.empty()) {
		material.pixelRoot32bitConstant.create(device, pixelRoot32bitSizes);
	}
}

//�e�N�X�`�����܂Ƃ߂Đ�������B�܂Ƃ߂đ���̂�CPU�I�[�o�[�w�b�h�����Ȃ�
void GpuResourceManager::createTextures(RefPtr<ID3D12Device> device, CommandContext& commandContext, const VectorArray<String>& settings) {
	VectorArray<ComPtr<ID3D12Resource>> uploadHeaps(settings.size());
	auto commandListSet = commandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	for (size_t i = 0; i < settings.size(); ++i) {
		String fullPath = "Resources/" + settings[i];

		//�e�N�X�`�����L���b�V���ɐ���
		auto itr =_resourcePool->_textures.emplace(std::piecewise_construct,
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
			seed ^= HashCombine(hash<float>()(p.normal.x), seed);
			seed ^= HashCombine(hash<float>()(p.normal.y), seed);
			seed ^= HashCombine(hash<float>()(p.normal.z), seed);
			seed ^= HashCombine(hash<float>()(p.texcoord.x), seed);
			seed ^= HashCombine(hash<float>()(p.texcoord.y), seed);
			return seed;
		}
	};
}

#include <fbxsdk.h>
using namespace fbxsdk;
void GpuResourceManager::createMeshSets(RefPtr<ID3D12Device> device, CommandContext & commandContext, const VectorArray<String>& fileNames) {
	VectorArray<ComPtr<ID3D12Resource>> uploadHeaps(fileNames.size() * 2);//�ǂݍ��ރt�@�C�����~(���_�o�b�t�@�{�C���f�b�N�X�o�b�t�@)
	auto commandListSet = commandContext.requestCommandListSet();
	ID3D12GraphicsCommandList* commandList = commandListSet.commandList;
	
	uint32 uploadHeapCounter = 0;
	for (const auto& fileName : fileNames) {
		fbxsdk::FbxManager* manager = fbxsdk::FbxManager::Create();
		FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
		manager->SetIOSettings(ios);
		FbxScene* scene = FbxScene::Create(manager, "");

		FbxImporter* importer = FbxImporter::Create(manager, "");
		String fullPath = "Resources/" + fileName;
		bool isSuccsess = importer->Initialize(fullPath.c_str(), -1, manager->GetIOSettings());
		assert(isSuccsess && "FBX�ǂݍ��ݎ��s");

		importer->Import(scene);
		importer->Destroy();

		FbxGeometryConverter geometryConverter(manager);
		geometryConverter.Triangulate(scene, true);

		FbxAxisSystem::DirectX.ConvertScene(scene);

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
		for (uint32 i = 0; i < polygonCount; ++i) {
			const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
			materialIndexSize[materialId] += polygonVertexCount;
		}

		UnorderedMap<RawVertex, uint32> optimizedVertices;//�d�����Ȃ����_���ƐV�������_�C���f�b�N�X
		VectorArray<UINT32> indices(indexCount);//�V�������_�C���f�b�N�X�łł����C���f�b�N�X�o�b�t�@
		VectorArray<uint32> materialIndexCounter(materialCount);//�}�e���A�����Ƃ̃C���f�b�N�X�����Ǘ�

		optimizedVertices.reserve(indexCount);

		for (uint32 i = 0; i < polygonCount; ++i) {
			const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
			uint32& indexCount = materialIndexCounter[materialId];

			for (uint32 j = 0; j < polygonVertexCount; ++j) {
				const uint32 vertexIndex = mesh->GetPolygonVertex(i, j);
				FbxVector4 v = mesh->GetControlPointAt(vertexIndex);
				FbxVector4 normal;
				FbxVector2 texcoord;

				FbxString uvSetName = uvSetNames.GetStringAt(0);//UVSet�͂O�ԃC���f�b�N�X�̂ݑΉ�
				mesh->GetPolygonVertexUV(i, j, uvSetName, texcoord, bIsUnmapped);
				mesh->GetPolygonVertexNormal(i, j, normal);

				RawVertex r;
				r.position = { (float)v[0], (float)v[1], -(float)v[2] };//FBX�͉E����W�n�Ȃ̂ō�����W�n�ɒ������߂�Z�𔽓]����
				r.normal = { (float)normal[0], (float)normal[1], -(float)normal[2] };
				r.texcoord = { (float)texcoord[0], 1 - (float)texcoord[1] };

				const Vector3 vectorUp = { 0.0f, 1, EPSILON };
				r.tangent = Vector3::cross(r.normal, vectorUp);

				//Z�𔽓]����ƃ|���S���������ɂȂ�̂ŉE���ɂȂ�悤�ɃC���f�b�N�X��0,1,2 �� 2,1,0�ɂ���
				const uint32 indexInverseCorrectionedValue = indexCount + 2 - j;
				if (optimizedVertices.count(r) == 0) {
					uint32 vertexIndex = static_cast<uint32>(optimizedVertices.size());
					indices[indexInverseCorrectionedValue] = vertexIndex;
					optimizedVertices.emplace(r, vertexIndex);
				}
				else {
					indices[indexInverseCorrectionedValue] = optimizedVertices.at(r);
				}

			}

			indexCount += polygonVertexCount;
		}

		//UnorederedMap�̔z�񂩂�VectorArray�ɕϊ�
		VectorArray<RawVertex> vertices(optimizedVertices.size());
		for (const auto& vertex : optimizedVertices) {
			vertices[vertex.second] = vertex.first;
		}

		manager->Destroy();

		//�}�e���A���X���b�g
		MaterialSlot slot;
		slot.indexCount = indexCount;
		slot.indexOffset = 0;

		//�L���b�V���ɃC���X�^���X�𐶐�
		const VectorArray<MaterialSlot> materialSlots = { slot };
		auto itr = _resourcePool->_meshes.emplace(std::piecewise_construct,
			std::make_tuple(fileName),
			std::make_tuple(materialSlots));

		MeshRenderSet& renderSet = (*itr.first).second;

		//���_�o�b�t�@����
		renderSet._vertexBuffer.createDeferred<RawVertex>(device, commandList, &uploadHeaps[uploadHeapCounter++], vertices);

		//�C���f�b�N�X�o�b�t�@
		renderSet._indexBuffer.createDeferred(device, commandList, &uploadHeaps[uploadHeapCounter++], indices);
	}

	//�A�b�v���[�h�o�b�t�@��GPU�I�����[�o�b�t�@�ɃR�s�[
	commandContext.executeCommandList(commandList);
	commandContext.discardCommandListSet(commandListSet);

	//�R�s�[���I���܂ŃA�b�v���[�h�q�[�v��j�����Ȃ�
	commandContext.waitForIdle();
}

void GpuResourceManager::loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial) const{
	assert(_resourcePool->_sharedMaterials.count(materialName) > 0 && "�}�e���A����������܂���");
	dstMaterial = &_resourcePool->_sharedMaterials.at(materialName);
}

void GpuResourceManager::loadTexture(const String & textureName, RefPtr<Texture2D>& dstTexture) const{
	assert(_resourcePool->_textures.count(textureName) > 0 && "�e�N�X�`����������܂���");
	dstTexture = &_resourcePool->_textures.at(textureName);
}

void GpuResourceManager::loadMeshSets(const String & meshName, RefPtr<MeshRenderSet>& dstMeshSet) const{
	assert(_resourcePool->_meshes.count(meshName) > 0 && "���b�V����������܂���");
	dstMeshSet = &_resourcePool->_meshes.at(meshName);
}

RefPtr<StaticSingleMeshRender> GpuResourceManager::createStaticSingleMeshRender(const String& name, const VectorArray<String>& materialNames) const{
	//���b�V���C���X�^���X����
	RefPtr<MeshRenderSet> renderSet;
	loadMeshSets(name, renderSet);
	StaticSingleMeshRender render(renderSet);

	//�}�e���A���X���b�g�Ƀ}�e���A�����Z�b�g
	for (size_t i = 0; i < materialNames.size(); ++i) {
		RefPtr<SharedMaterial> material;
		loadSharedMaterial(materialNames[i], material);
		render.setMaterial(static_cast<uint32>(i), material);
	}

	auto& renderList = _resourcePool->_renderList;
	renderList.emplace_back(std::move(render));

	return &renderList.back();
}

void GpuResourceManager::removeStaticSingleMeshRender(RefPtr<StaticSingleMeshRender> render){
}

void GpuResourceManager::shutdown() {
	_resourcePool.reset();
}

const ListArray<StaticSingleMeshRender>& GpuResourceManager::getMeshes() const{
	return _resourcePool->_renderList;
}
