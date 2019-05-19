#include <iostream>
#include <fstream>
#include <cassert>
#include <fbxsdk.h>
#include <LMath.h>
#include <Utility.h>
using namespace fbxsdk;

struct RawVertex {
	Vector3 position;
	Vector3 normal;
	Vector3 tangent;
	Vector2 texcoord;

	bool operator==(const RawVertex& left) const {
		return position == left.position && texcoord == texcoord;
	}
};

struct MaterialDrawRange {
	MaterialDrawRange(uint32 indexCount, uint32 indexOffset) :indexCount(indexCount), indexOffset(indexOffset) {}

	uint32 indexCount;
	uint32 indexOffset;
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

int main(int argc, char* argv[]) {
	std::cout << argc << std::endl;

	uint32 fileCount = argc - 1;
	VectorArray<String> fileNames(fileCount);

	for (int i = 0; i < fileCount; ++i) {
		fileNames[i] = argv[i + 1];
		std::cout <<"File: "<< fileNames[i] << std::endl;
	}


	for (const auto& fileName : fileNames) {
		fbxsdk::FbxManager* manager = fbxsdk::FbxManager::Create();
		FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
		manager->SetIOSettings(ios);
		FbxScene* scene = FbxScene::Create(manager, "");

		FbxImporter* importer = FbxImporter::Create(manager, "");
		String fullPath = fileName;
		bool isSuccsess = importer->Initialize(fullPath.c_str(), -1, manager->GetIOSettings());
		assert(isSuccsess && "FBX読み込み失敗");

		importer->Import(scene);
		importer->Destroy();

		FbxGeometryConverter geometryConverter(manager);
		geometryConverter.Triangulate(scene, true);

		FbxAxisSystem::DirectX.ConvertScene(scene);

		FbxMesh* mesh = scene->GetMember<FbxMesh>(0);
		const uint32 materialCount = scene->GetMaterialCount();
		const uint32 vertexCount = mesh->GetControlPointsCount();
		const uint32 polygonCount = mesh->GetPolygonCount();
		const uint32 polygonVertexCount = 3;
		const uint32 indexCount = polygonCount * polygonVertexCount;

		FbxStringList uvSetNames;
		bool bIsUnmapped = false;
		mesh->GetUVSetNames(uvSetNames);

		FbxLayerElementMaterial* meshMaterials = mesh->GetLayer(0)->GetMaterials();

		//マテリアルごとの頂点インデックス数を調べる
		VectorArray<uint32> materialIndexSizes(materialCount);
		for (uint32 i = 0; i < polygonCount; ++i) {
			const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
			materialIndexSizes[materialId] += polygonVertexCount;
		}

		//マテリアルごとのインデックスオフセットを計算
		VectorArray<uint32> materialIndexOffsets(materialCount);
		for (size_t i = 0; i < materialIndexOffsets.size(); ++i) {
			for (size_t j = 0; j < i; ++j) {
				materialIndexOffsets[i] += materialIndexSizes[j];
			}
		}

		UnorderedMap<RawVertex, uint32> optimizedVertices;//重複しない頂点情報と新しい頂点インデックス
		VectorArray<uint32> indices(indexCount);//新しい頂点インデックスでできたインデックスバッファ
		VectorArray<uint32> materialIndexCounter(materialCount);//マテリアルごとのインデックス数を管理

		optimizedVertices.reserve(indexCount);

		for (uint32 i = 0; i < polygonCount; ++i) {
			const uint32 materialId = meshMaterials->GetIndexArray().GetAt(i);
			const uint32 materialIndexOffset = materialIndexOffsets[materialId];
			uint32& indexCount = materialIndexCounter[materialId];

			for (uint32 j = 0; j < polygonVertexCount; ++j) {
				const uint32 vertexIndex = mesh->GetPolygonVertex(i, j);
				FbxVector4 v = mesh->GetControlPointAt(vertexIndex);
				FbxVector4 normal;
				FbxVector2 texcoord;

				FbxString uvSetName = uvSetNames.GetStringAt(0);//UVSetは０番インデックスのみ対応
				mesh->GetPolygonVertexUV(i, j, uvSetName, texcoord, bIsUnmapped);
				mesh->GetPolygonVertexNormal(i, j, normal);

				RawVertex r;
				r.position = { (float)v[0], (float)v[1], -(float)v[2] };//FBXは右手座標系なので左手座標系に直すためにZを反転する
				r.normal = { (float)normal[0], (float)normal[1], -(float)normal[2] };
				r.texcoord = { (float)texcoord[0], 1 - (float)texcoord[1] };//UVのY軸を反転

				const Vector3 vectorUp = { 0.0f, 1, EPSILON };
				r.tangent = Vector3::cross(r.normal, vectorUp);

				//Zを反転するとポリゴンが左回りになるので右回りになるようにインデックスを0,1,2 → 2,1,0にする
				const uint32 indexInverseCorrectionedValue = indexCount + 2 - j;
				const uint32 indexPerMaterial = materialIndexOffset + indexInverseCorrectionedValue;
				if (optimizedVertices.count(r) == 0) {
					uint32 vertexIndex = static_cast<uint32>(optimizedVertices.size());
					indices[indexPerMaterial] = vertexIndex;
					optimizedVertices.emplace(r, vertexIndex);
				}
				else {
					indices[indexPerMaterial] = optimizedVertices.at(r);
				}

			}

			indexCount += polygonVertexCount;
		}

		//UnorederedMapの配列からVectorArrayに変換
		VectorArray<RawVertex> vertices(optimizedVertices.size());
		for (const auto& vertex : optimizedVertices) {
			vertices[vertex.second] = vertex.first;
		}

		manager->Destroy();

		//マテリアルの描画範囲を設定
		VectorArray<MaterialDrawRange> materialRanges;
		materialRanges.reserve(materialCount);

		for (size_t i = 0; i < materialCount; ++i) {
			materialRanges.emplace_back(materialIndexSizes[i], materialIndexOffsets[i]);
		}

		//BoundingBoxのサイズを計算
		Vector3 aabbMin;
		Vector3 aabbMax;
		for (auto&& v : vertices) {
			const Vector3& p = v.position;
			if (p.x < aabbMin.x) { aabbMin.x = p.x; }
			if (p.x > aabbMax.x) { aabbMax.x = p.x; }
			if (p.y < aabbMin.y) { aabbMin.y = p.y; }
			if (p.y > aabbMax.y) { aabbMax.y = p.y; }
			if (p.z < aabbMin.z) { aabbMin.z = p.z; }
			if (p.z > aabbMax.z) { aabbMax.z = p.z; }
		}

		uint32 allFileSize = 0;
		uint32 verticesCount = vertices.size();
		uint32 indicesCount = indices.size();
		uint32 verticesSize = verticesCount * sizeof(RawVertex);
		uint32 indicesSize = indicesCount * sizeof(uint32);
		uint32 materialSize = materialCount * sizeof(MaterialDrawRange);
		uint32 boundingBoxSize = sizeof(Vector3) * 2;

		allFileSize += verticesSize;
		allFileSize += indicesSize;
		allFileSize += materialSize;
		allFileSize += boundingBoxSize;

		std::string filePath(fileName.c_str());
		size_t splitPoint = filePath.find('.');
		filePath = filePath.substr(0, splitPoint);
		filePath.append(".mesh");

		std::ofstream fout;
		fout.open(filePath, std::ios::out | std::ios::binary | std::ios::trunc);

		fout.write(reinterpret_cast<const char*>(&allFileSize), 4);
		fout.write(reinterpret_cast<const char*>(&verticesCount), 4);
		fout.write(reinterpret_cast<const char*>(&indicesCount), 4);
		fout.write(reinterpret_cast<const char*>(&materialCount), 4);
		fout.write(reinterpret_cast<const char*>(vertices.data()), verticesSize);
		fout.write(reinterpret_cast<const char*>(indices.data()), indicesSize);
		fout.write(reinterpret_cast<const char*>(materialRanges.data()), materialSize);
		fout.write(reinterpret_cast<const char*>(&aabbMin), 12);
		fout.write(reinterpret_cast<const char*>(&aabbMax), 12);

		fout.close();
	}

	return 0;
}