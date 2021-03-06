#include "Win32Application.h"
#include "GFXInterface.h"
#include <LMath.h>
#include <ThirdParty/Imgui/imgui.h>
#include <SharedMaterial.h>
#include <Scene.h>
#include <GraphicsCore.h>
#include <fstream>

VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
};

class TestScene_Gun :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String meshName("Cerberus/Cerberus.mesh");
		String skyName("skySphere.mesh");
		String diffuseEnv("cubemapEnvHDR.dds");
		String specularEnv("cubemapSpecularHDR.dds");
		String specularBrdf("cubemapBrdf.dds");

		String albedo("Cerberus/Cerberus_A.dds");
		String normal("Cerberus/Cerberus_N.dds");
		String metallic("Cerberus/Cerberus_M.dds");
		String roughness("Cerberus/Cerberus_R.dds");
		String ao("Cerberus/Cerberus_AO.dds");

		//テクスチャ読み込み
		gfx.createTextures({ albedo, normal, metallic, roughness, ao, diffuseEnv, specularEnv, specularBrdf });

		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "TestM";
		materialSettings.vertexShaderName = "shader_vs.hlsl";
		materialSettings.pixelShaderName = "shader_ps.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = { diffuseEnv, specularEnv, specularBrdf, albedo, normal, metallic, roughness, ao };
		materialSettings.inputLayouts = inputLayouts;
		materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(materialSettings);

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		skyMatSettings.inputLayouts = inputLayouts;
		skyMatSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ meshName,skyName });
		_mesh.loadInstance(meshName, { "TestM" });
		_sky.loadInstance(skyName, { "TestS" });
	}

	void onUpdate() override {
		Scene::onUpdate();

		static float x = 0.0f;
		static float y = 0.0f;
		static float z = 0.8f;
		static float pitch = 0;
		static float yaw = 0;
		static float roll = 0;

		static float pitchL = 1.0f;
		static float yawL = 0.2f;
		static float rollL = 0;
		static Vector3 color = Vector3::one;
		static float intensity = 1.0f;

		ImGui::Begin("TestD3D12");
		ImGui::SliderFloat("World X", &x, -1, 10);
		ImGui::SliderFloat("World Y", &y, -1, 10);
		ImGui::SliderFloat("World Z", &z, -1, 10);
		ImGui::SliderAngle("Picth", &pitch);
		ImGui::SliderAngle("Yaw", &yaw);
		ImGui::SliderAngle("Roll", &roll);
		ImGui::End();

		ImGui::Begin("DirectionalLight");
		ImGui::SliderAngle("Picth", &pitchL);
		ImGui::SliderAngle("Yaw", &yawL);
		ImGui::SliderAngle("Roll", &rollL);
		ImGui::SliderFloat("Intensity", &intensity, 0, 10);
		ImGui::ColorEdit3("Color", (float*)& color);
		ImGui::End();

		GFXInterface& gfx = GFXInterface::instance();

		Matrix4 mtxWorld = Matrix4::matrixFromQuaternion(Quaternion::euler({ pitch, yaw, roll }, true)).multiply(Matrix4::translateXYZ({ x, y, z }));

		_mesh.updateWorldMatrix(mtxWorld);
		//_mesh.getMaterial(0)->setParameter<Vector3>("direction", Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward));
		//_mesh.getMaterial(0)->setParameter<Vector3>("color", color);
		//_mesh.getMaterial(0)->setParameter<float>("intensity", intensity);

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky.updateWorldMatrix(skyMtxWorld);
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	SingleMeshRenderInstance _sky;
	SingleMeshRenderInstance _mesh;
};


class TestScene_ShaderBall :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String shaderBallName("ShaderBall/shaderBall.mesh");
		String skyName("skySphere.mesh");
		String diffuseEnv("cubemapEnvHDR.dds");
		String specularEnv("cubemapSpecularHDR.dds");
		String specularBrdf("cubemapBrdf.dds");

		String albedo("ShaderBall/bamboo-wood-semigloss-albedo.dds");
		String normal("ShaderBall/bamboo-wood-semigloss-normal.dds");
		String metallic("ShaderBall/bamboo-wood-semigloss-metal.dds");
		String roughness("ShaderBall/bamboo-wood-semigloss-roughness.dds");
		String ao("ShaderBall/bamboo-wood-semigloss-ao.dds");

		//テクスチャ読み込み
		gfx.createTextures({ albedo, normal, metallic, roughness, ao, diffuseEnv, specularEnv, specularBrdf });

		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "TestM";
		materialSettings.vertexShaderName = "shader_vs.hlsl";
		materialSettings.pixelShaderName = "shader_ps.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = { diffuseEnv, specularEnv, specularBrdf, albedo, normal, metallic, roughness, ao };
		materialSettings.inputLayouts = inputLayouts;
		materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(materialSettings);

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		skyMatSettings.inputLayouts = inputLayouts;
		skyMatSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ shaderBallName,skyName });
		_mesh.loadInstance(shaderBallName, { "TestM" });
		_sky.loadInstance(skyName, { "TestS" });
	}

	void onUpdate() override {
		Scene::onUpdate();

		static float x = 0.0f;
		static float y = -0.65f;
		static float z = 1.7f;
		static float pitch = 0;
		static float yaw = 0;
		static float roll = 0;

		static float pitchL = 1.0f;
		static float yawL = 0.2f;
		static float rollL = 0;
		static Vector3 color = Vector3::one;
		static float intensity = 1.0f;

		{
			ImGui::Begin("TestD3D12");
			ImGui::Text("Lightn");
			ImGui::SliderFloat("World X", &x, -1, 10);
			ImGui::SliderFloat("World Y", &y, -1, 10);
			ImGui::SliderFloat("World Z", &z, -1, 10);
			ImGui::SliderAngle("Picth", &pitch);
			ImGui::SliderAngle("Yaw", &yaw);
			ImGui::SliderAngle("Roll", &roll);
			ImGui::End();

			ImGui::Begin("DirectionalLight");
			ImGui::SliderAngle("Picth", &pitchL);
			ImGui::SliderAngle("Yaw", &yawL);
			ImGui::SliderAngle("Roll", &rollL);
			ImGui::SliderFloat("Intensity", &intensity, 0, 10);
			ImGui::ColorEdit3("Color", (float*)& color);
			ImGui::End();
		}

		GFXInterface& gfx = GFXInterface::instance();

		Matrix4 mtxWorld = Matrix4::matrixFromQuaternion(Quaternion::euler({ pitch, yaw, roll }, true)).multiply(Matrix4::translateXYZ({ x, y, z }));

		_mesh.updateWorldMatrix(mtxWorld);
		//_mesh.getMaterial(0)->setParameter<Vector3>("direction", Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward));
		//_mesh.getMaterial(0)->setParameter<Vector3>("color", color);
		//_mesh.getMaterial(0)->setParameter<float>("intensity", intensity);

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky.updateWorldMatrix(skyMtxWorld);
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	SingleMeshRenderInstance _sky;
	SingleMeshRenderInstance _mesh;
};


class TestScene_PBRBalls :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String meshName("sphere.mesh");
		String skyName("skySphere.mesh");
		String diffuseEnv("cubemapEnvHDR.dds");
		String specularEnv("cubemapSpecularHDR.dds");
		String specularBrdf("cubemapBrdf.dds");

		//テクスチャ読み込み
		gfx.createTextures({ diffuseEnv, specularEnv, specularBrdf });

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		skyMatSettings.inputLayouts = inputLayouts;
		skyMatSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ meshName,skyName });

		uint32 meshesLength = xNum * yNum;
		_meshes.resize(meshesLength);
		for (uint32 x = 0; x < xNum; ++x) {
			for (uint32 y = 0; y < yNum; ++y) {
				String matName("Test");
				matName.append((std::to_string(x) + "" + std::to_string(y)).c_str());

				SharedMaterialCreateSettings materialSettings;
				materialSettings.name = matName;
				materialSettings.vertexShaderName = "shader_vs.hlsl";
				materialSettings.pixelShaderName = "pbr_gradation_ps.hlsl";
				materialSettings.vsTextures = {};
				materialSettings.psTextures = { diffuseEnv, specularEnv, specularBrdf };
				materialSettings.inputLayouts = inputLayouts;
				materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				gfx.createSharedMaterial(materialSettings);

				uint32 arrayIndex = x * xNum + y;
				_meshes[arrayIndex].loadInstance(meshName, { matName });
			}
		}

		_sky.loadInstance(skyName, { "TestS" });
	}

	void onUpdate() override {
		Scene::onUpdate();

		static float x = 0.0f;
		static float y = 0.0f;
		static float z = 7.0f;
		static float pitch = 0;
		static float yaw = 0;
		static float roll = 0;

		static float pitchL = 1.0f;
		static float yawL = 0.2f;
		static float rollL = 0;
		static Vector3 color = Vector3::one;
		static float intensity = 0.0f;

		{
			ImGui::Begin("TestD3D12");
			ImGui::Text("Lightn");
			ImGui::SliderFloat("World X", &x, -1, 10);
			ImGui::SliderFloat("World Y", &y, -1, 10);
			ImGui::SliderFloat("World Z", &z, -1, 10);
			ImGui::SliderAngle("Picth", &pitch);
			ImGui::SliderAngle("Yaw", &yaw);
			ImGui::SliderAngle("Roll", &roll);
			ImGui::End();

			ImGui::Begin("DirectionalLight");
			ImGui::SliderAngle("Picth", &pitchL);
			ImGui::SliderAngle("Yaw", &yawL);
			ImGui::SliderAngle("Roll", &rollL);
			ImGui::SliderFloat("Intensity", &intensity, 0, 10);
			ImGui::ColorEdit3("Color", (float*)& color);
			ImGui::End(); 
		}

		GFXInterface& gfx = GFXInterface::instance();

		Matrix4 mtxWorld = Matrix4::matrixFromQuaternion(Quaternion::euler({ pitch, yaw, roll }, true)).multiply(Matrix4::translateXYZ({ x, y, z }));

		Vector3 lightDir = Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward);
		for (uint32 x = 0; x < xNum; ++x) {
			for (uint32 y = 0; y < yNum; ++y) {
				Vector3 offset(x - (float)(xNum / 2.0f)+0.5f, y - (float)(yNum / 2.0f)+0.5f, 0);
				uint32 index = x * xNum + y;
				float metallic = y / (float)(yNum-1);
				float roughness = x / (float)(xNum-1);
				_meshes[index].updateWorldMatrix(mtxWorld.multiply(Matrix4::translateXYZ(offset)));
				//_meshes[index].getMaterial(0)->setParameter<Vector3>("direction", lightDir);
				//_meshes[index].getMaterial(0)->setParameter<Vector3>("color", color);
				//_meshes[index].getMaterial(0)->setParameter<float>("intensity", intensity);
				//_meshes[index].getMaterial(0)->setParameter<float>("p_metallic", metallic);
				//_meshes[index].getMaterial(0)->setParameter<float>("p_roughness", roughness);
			}
		}

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky.updateWorldMatrix(skyMtxWorld);
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	SingleMeshRenderInstance _sky;
	
	const uint32 xNum = 7;
	const uint32 yNum = 7;
	VectorArray<SingleMeshRenderInstance> _meshes;
};


class TestScene_MultiMaterial :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String meshName("MultiMaterial.mesh");
		String skyName("skySphere.mesh");
		String diffuseEnv("cubemapEnvHDR.dds");
		String specularEnv("cubemapSpecularHDR.dds");
		String specularBrdf("cubemapBrdf.dds");

		String albedo("ShaderBall/bamboo-wood-semigloss-albedo.dds");
		String normal("ShaderBall/bamboo-wood-semigloss-normal.dds");
		String metallic("ShaderBall/bamboo-wood-semigloss-metal.dds");
		String roughness("ShaderBall/bamboo-wood-semigloss-roughness.dds");
		String ao("ShaderBall/bamboo-wood-semigloss-ao.dds");

		String albedo2("Cerberus/Cerberus_A.dds");
		String normal2("Cerberus/Cerberus_N.dds");
		String metallic2("Cerberus/Cerberus_M.dds");
		String roughness2("Cerberus/Cerberus_R.dds");
		String ao2("Cerberus/Cerberus_AO.dds");

		//テクスチャ読み込み
		gfx.createTextures({ albedo, normal, metallic, roughness, ao, albedo2, normal2, metallic2, roughness2, ao2,
			diffuseEnv, specularEnv, specularBrdf });

		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "TestM";
		materialSettings.vertexShaderName = "shader_vs.hlsl";
		materialSettings.pixelShaderName = "shader_ps.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = { diffuseEnv, specularEnv, specularBrdf, albedo, normal, metallic, roughness, ao };
		materialSettings.inputLayouts = inputLayouts;
		materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(materialSettings);

		SharedMaterialCreateSettings materialSettings2;
		materialSettings2.name = "TestM2";
		materialSettings2.vertexShaderName = "shader_vs.hlsl";
		materialSettings2.pixelShaderName = "shader_ps.hlsl";
		materialSettings2.vsTextures = {};
		materialSettings2.psTextures = { diffuseEnv, specularEnv, specularBrdf, albedo2, normal2, metallic2, roughness2, ao2 };
		materialSettings2.inputLayouts = inputLayouts;
		materialSettings2.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(materialSettings2);

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		skyMatSettings.inputLayouts = inputLayouts;
		skyMatSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ meshName,skyName });
		_mesh.loadInstance(meshName, { "TestM","TestM","TestM2" });
		_sky.loadInstance(skyName, { "TestS" });
	}

	void onUpdate() override {
		Scene::onUpdate();

		static float x = 0.0f;
		static float y = -0.65f;
		static float z = 5.7f;
		static float pitch = 0;
		static float yaw = 0;
		static float roll = 0;

		static float pitchL = 1.0f;
		static float yawL = 0.2f;
		static float rollL = 0;
		static Vector3 color = Vector3::one;
		static float intensity = 1.0f;

		{
			ImGui::Begin("TestD3D12");
			ImGui::Text("Lightn");
			ImGui::SliderFloat("World X", &x, -1, 10);
			ImGui::SliderFloat("World Y", &y, -1, 10);
			ImGui::SliderFloat("World Z", &z, -1, 10);
			ImGui::SliderAngle("Picth", &pitch);
			ImGui::SliderAngle("Yaw", &yaw);
			ImGui::SliderAngle("Roll", &roll);
			ImGui::End();

			ImGui::Begin("DirectionalLight");
			ImGui::SliderAngle("Picth", &pitchL);
			ImGui::SliderAngle("Yaw", &yawL);
			ImGui::SliderAngle("Roll", &rollL);
			ImGui::SliderFloat("Intensity", &intensity, 0, 10);
			ImGui::ColorEdit3("Color", (float*)& color);
			ImGui::End();
		}

		GFXInterface& gfx = GFXInterface::instance();

		Matrix4 mtxWorld = Matrix4::matrixFromQuaternion(Quaternion::euler({ pitch, yaw, roll }, true)).multiply(Matrix4::translateXYZ({ x, y, z }));

		_mesh.updateWorldMatrix(mtxWorld);
		for (auto&& material : _mesh.getMaterials()) {
			//material->setParameter<Vector3>("direction", Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward));
			//material->setParameter<Vector3>("color", color);
			//material->setParameter<float>("intensity", intensity);
		}

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky.updateWorldMatrix(skyMtxWorld);
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	SingleMeshRenderInstance _sky;
	SingleMeshRenderInstance _mesh;
};


class TestScene_DebugGeometrys :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String skyName("skySphere.mesh");
		String diffuseEnv("cubemapEnvHDR.dds");
		String specularEnv("cubemapSpecularHDR.dds");
		String specularBrdf("cubemapBrdf.dds");

		//テクスチャ読み込み
		//gfx.createTextures({ diffuseEnv, specularEnv, specularBrdf });

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		skyMatSettings.inputLayouts = inputLayouts;
		skyMatSettings.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		//gfx.createMeshSets({ skyName });
		//_sky.loadInstance(skyName, { "TestS" });
	}

	void onUpdate() override {
		Scene::onUpdate();

		static float sphereRadius = 1;

		static float capsuleRadius = 1;
		static float capsuleHeight = 1;

		static float sphereColor[4] = { 0,1,0,1 };
		static Vector3 lineStartPosition = Vector3::zero;
		static Vector3 lineEndPosition = Vector3::up;
		static Vector3 cubeRotation = Vector3::zero;
		static Vector3 cubeExtent = Vector3::one;

		{
			ImGui::Begin("DebugGeometry");
			ImGui::DragFloat3("LineStart", (float*)& lineStartPosition, 0.1f);
			ImGui::DragFloat3("LineEnd", (float*)& lineEndPosition, 0.1f);
			ImGui::DragFloat3("CubeExtent", (float*)& cubeExtent, 0.1f);
			ImGui::SliderAngle("CubeRotateX", &cubeRotation.x);
			ImGui::SliderAngle("CubeRotateY", &cubeRotation.y);
			ImGui::SliderAngle("CubeRotateZ", &cubeRotation.z);
			ImGui::SliderFloat("CapsuleRadius", &capsuleRadius, 0, 10);
			ImGui::SliderFloat("CapsuleHeight", &capsuleHeight, 0, 10);
			ImGui::SliderFloat("SphereRadius", &sphereRadius, 0, 10);
			ImGui::ColorEdit4("SphereColor", sphereColor);
			ImGui::End();
		}

		RefPtr<DebugGeometryRender> debugGeometryRender = GFXInterface::instance()._graphicsCore->getDebugGeometryRender();

		Vector3 offset = Vector3::forward * 5;
		debugGeometryRender->debugDrawCube(Vector3(0, 0, 0) + offset, Quaternion::euler(cubeRotation, true), cubeExtent, Color::white);

		debugGeometryRender->debugDrawLine(Vector3(0, 0, 0) + offset, Vector3(1, 0, 0) + offset, Color::red);
		debugGeometryRender->debugDrawLine(Vector3(0, 0, 0) + offset, Vector3(0, 0, 1) + offset, Color::blue);
		debugGeometryRender->debugDrawLine(lineStartPosition + offset, lineEndPosition + offset, Color::green);

		debugGeometryRender->debugDrawSphere(Vector3(-1, 0, 0) + offset, Quaternion::identity, sphereRadius, *((Color*)sphereColor));

		debugGeometryRender->debugDrawCapsule(Vector3(-2.5f, 0, 0) + offset, Quaternion::identity, capsuleRadius, capsuleHeight, Color::blue);

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		//_sky.updateWorldMatrix(skyMtxWorld);
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	SingleMeshRenderInstance _sky;
};


class TestScene_StaticMultiMesh :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String skyName("skySphere.mesh");
		String diffuseEnv("cubemapEnvHDR.dds");
		String specularEnv("cubemapSpecularHDR.dds");
		String specularBrdf("cubemapBrdf.dds");

		//テクスチャ読み込み
		gfx.createTextures({ diffuseEnv, specularEnv, specularBrdf });

		String fullPath = "Resources/Environment/meshes.scene";
		std::ifstream fin(fullPath.c_str(), std::ios::in | std::ios::binary);
		fin.exceptions(std::ios::badbit);

		assert(!fin.fail() && "メッシュファイルが読み込めません");

		char materialName[64];
		uint32 textureCount;
		uint32 meshCount;

		fin.read(reinterpret_cast<char*>(materialName), 64);
		fin.read(reinterpret_cast<char*>(&textureCount), 4);
		fin.read(reinterpret_cast<char*>(&meshCount), 4);

		InitSettingsPerStaticMultiMesh initSettings;
		initSettings.textureNames.resize(textureCount);
		initSettings.meshNames.resize(meshCount);
		initSettings.meshes.resize(meshCount);

		for (uint32 i = 0; i < textureCount; ++i) {
			char textureName[64];
			fin.read(reinterpret_cast<char*>(textureName), 64);

			initSettings.textureNames[i] = String("Environment/").append(String(textureName)).append(".dds");
		}

		for (uint32 i = 0; i < meshCount; ++i) {
			uint32 subMeshCount;
			uint32 instanceCount;
			char meshName[64];
			fin.read(reinterpret_cast<char*>(meshName), 64);
			fin.read(reinterpret_cast<char*>(&instanceCount), 4);
			fin.read(reinterpret_cast<char*>(&subMeshCount), 4);

			initSettings.meshNames[i] = String("Environment/").append(String(meshName)).append(".mesh");

			PerMeshData& meshData = initSettings.meshes[i];
			meshData.matrices.resize(instanceCount);
			meshData.textureIndices.resize(subMeshCount);

			//テクスチャ３枚読み込み
			for (uint32 j = 0; j < subMeshCount; ++j) {
				fin.read(reinterpret_cast<char*>(&meshData.textureIndices[j]), 12);
				//fin.read(reinterpret_cast<char*>(&meshData.textureIndices[j]), 16);
			}

			for (uint32 j = 0; j < instanceCount; ++j) {
				Vector3 position;
				Quaternion rotation;
				Vector3 scale;
				fin.read(reinterpret_cast<char*>(&position), 12);
				fin.read(reinterpret_cast<char*>(&rotation), 16);
				fin.read(reinterpret_cast<char*>(&scale), 12);

				meshData.matrices[j] = Matrix4::createWorldMatrix(position, rotation, scale);
			}
		}

		fin.close();

		RefPtr<GraphicsCore> graphicsCore = GFXInterface::instance()._graphicsCore.get();

		graphicsCore->createMeshSets(initSettings.meshNames);
		graphicsCore->createTextures(initSettings.textureNames);
		graphicsCore->createStaticMultiMeshRender("mul",initSettings);
	}

	void onUpdate() override {
		Scene::onUpdate();
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	//SingleMeshRenderInstance _sky;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	Win32Application app;
	app.init(hInstance, nCmdShow);
	app._sceneManager->changeScene<TestScene_StaticMultiMesh>();

	return app.run();
}