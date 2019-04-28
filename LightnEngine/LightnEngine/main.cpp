#include "Win32Application.h"
#include "GFXInterface.h"
#include <LMath.h>
#include <ThirdParty/Imgui/imgui.h>
#include <ImguiWindow.h>
#include <RenderableEntity.h>
#include <SharedMaterial.h>
#include <GpuResource.h>
#include <Scene.h>

VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
		gfx.createSharedMaterial(materialSettings);

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		skyMatSettings.inputLayouts = inputLayouts;
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ meshName,skyName });
		_mesh = gfx.createStaticSingleMeshRender(meshName, { "TestM" });
		_sky = gfx.createStaticSingleMeshRender(skyName, { "TestS" });
	}

	void onUpdate() override {
		Scene::onUpdate();

		static float x = 0.0f;
		static float y = 0.0f;
		static float z = 0.8f;
		static float pitch = 0;
		static float yaw = 0;
		static float roll = 0;

		static float xC = 0.0f;
		static float yC = 0.0f;
		static float zC = 0.0f;
		static float pitchC = 0;
		static float yawC = 0;
		static float rollC = 0;

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

		ImGui::Begin("Camera");
		ImGui::SliderFloat("World X", &xC, -5, 5);
		ImGui::SliderFloat("World Y", &yC, -5, 5);
		ImGui::SliderFloat("World Z", &zC, -5, 5);
		ImGui::SliderAngle("Picth", &pitchC);
		ImGui::SliderAngle("Yaw", &yawC);
		ImGui::SliderAngle("Roll", &rollC);
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
		Matrix4 mtxView = Matrix4::matrixFromQuaternion(Quaternion::euler({ pitchC, yawC, rollC }, true)).multiply(Matrix4::translateXYZ({ xC, yC, zC })).inverse();
		Matrix4 mtxProj = Matrix4::perspectiveFovLH(radianFromDegree(60), gfx.getWidth() / static_cast<float>(gfx.getHeight()), 0.01f, 1000);
		//mtxProj = Matrix4::identity;

		_mesh.updateWorldMatrix(mtxWorld.transpose());
		_mesh.getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_mesh.getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
		_mesh.getMaterial(0)->setParameter<Vector3>("cameraPos", Vector3(xC, yC, zC));
		_mesh.getMaterial(0)->setParameter<Vector3>("direction", Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward));
		_mesh.getMaterial(0)->setParameter<Vector3>("color", color);
		_mesh.getMaterial(0)->setParameter<float>("intensity", intensity);

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky.updateWorldMatrix(skyMtxWorld.transpose());
		_sky.getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_sky.getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	StaticSingleMeshRender _sky;
	StaticSingleMeshRender _mesh;
};


class TestScene_ShaderBall :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String shaderBallName("ShaderBall/shaderBall.fbx");
		String skyName("skySphere.fbx");
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
		gfx.createSharedMaterial(materialSettings);

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		skyMatSettings.inputLayouts = inputLayouts;
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ shaderBallName,skyName });
		_mesh = gfx.createStaticSingleMeshRender(shaderBallName, { "TestM" });
		_sky = gfx.createStaticSingleMeshRender(skyName, { "TestS" });
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
		Matrix4 mtxView = Matrix4::identity;
		Matrix4 mtxProj = Matrix4::perspectiveFovLH(radianFromDegree(60), gfx.getWidth() / static_cast<float>(gfx.getHeight()), 0.01f, 1000);
		//mtxProj = Matrix4::identity;

		_mesh.updateWorldMatrix(mtxWorld.transpose());
		_mesh.getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_mesh.getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
		_mesh.getMaterial(0)->setParameter<Vector3>("direction", Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward));
		_mesh.getMaterial(0)->setParameter<Vector3>("color", color);
		_mesh.getMaterial(0)->setParameter<float>("intensity", intensity);

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky.updateWorldMatrix(skyMtxWorld.transpose());
		_sky.getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_sky.getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	StaticSingleMeshRender _sky;
	StaticSingleMeshRender _mesh;
};


class TestScene_PBRBalls :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String meshName("sphere.fbx");
		String shaderBallName("ShaderBall/shaderBall.fbx");
		String skyName("skySphere.fbx");
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
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ meshName,skyName });

		_meshes.resize(xNum * yNum);
		for (int x = 0; x < xNum; ++x) {
			for (int y = 0; y < yNum; ++y) {
				String matName("Test");
				matName.append((std::to_string(x) + "" + std::to_string(y)).c_str());

				SharedMaterialCreateSettings materialSettings;
				materialSettings.name = matName;
				materialSettings.vertexShaderName = "shader_vs.hlsl";
				materialSettings.pixelShaderName = "pbr_gradation_ps.hlsl";
				materialSettings.vsTextures = {};
				materialSettings.psTextures = { diffuseEnv, specularEnv, specularBrdf };
				materialSettings.inputLayouts = inputLayouts;
				gfx.createSharedMaterial(materialSettings);

				_meshes[x * xNum + y] = gfx.createStaticSingleMeshRender(meshName, { matName });
			}
		}

		_sky = gfx.createStaticSingleMeshRender(skyName, { "TestS" });
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
		Matrix4 mtxView = Matrix4::identity;
		Matrix4 mtxProj = Matrix4::perspectiveFovLH(radianFromDegree(60), gfx.getWidth() / static_cast<float>(gfx.getHeight()), 0.01f, 1000);
		//mtxProj = Matrix4::identity;

		Vector3 lightDir = Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward);
		for (int x = 0; x < xNum; ++x) {
			for (int y = 0; y < yNum; ++y) {
				Vector3 offset(x - (float)(xNum / 2.0f)+0.5f, y - (float)(yNum / 2.0f)+0.5f, 0);
				uint32 index = x * xNum + y;
				float metallic = y / (float)(yNum-1);
				float roughness = x / (float)(xNum-1);
				_meshes[index].updateWorldMatrix(mtxWorld.multiply(Matrix4::translateXYZ(offset)).transpose());
				_meshes[index].getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
				_meshes[index].getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
				_meshes[index].getMaterial(0)->setParameter<Vector3>("direction", lightDir);
				_meshes[index].getMaterial(0)->setParameter<Vector3>("color", color);
				_meshes[index].getMaterial(0)->setParameter<float>("intensity", intensity);
				_meshes[index].getMaterial(0)->setParameter<float>("p_metallic", metallic);
				_meshes[index].getMaterial(0)->setParameter<float>("p_roughness", roughness);
			}
		}

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky.updateWorldMatrix(skyMtxWorld.transpose());
		_sky.getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_sky.getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	StaticSingleMeshRender _sky;
	
	const uint32 xNum = 7;
	const uint32 yNum = 7;
	VectorArray<StaticSingleMeshRender> _meshes;
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
		gfx.createSharedMaterial(materialSettings);

		SharedMaterialCreateSettings materialSettings2;
		materialSettings2.name = "TestM2";
		materialSettings2.vertexShaderName = "shader_vs.hlsl";
		materialSettings2.pixelShaderName = "shader_ps.hlsl";
		materialSettings2.vsTextures = {};
		materialSettings2.psTextures = { diffuseEnv, specularEnv, specularBrdf, albedo2, normal2, metallic2, roughness2, ao2 };
		materialSettings2.inputLayouts = inputLayouts;
		gfx.createSharedMaterial(materialSettings2);

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		skyMatSettings.inputLayouts = inputLayouts;
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ meshName,skyName });
		_mesh = gfx.createStaticSingleMeshRender(meshName, { "TestM","TestM","TestM2" });
		_sky = gfx.createStaticSingleMeshRender(skyName, { "TestS" });
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
		Matrix4 mtxView = Matrix4::identity;
		Matrix4 mtxProj = Matrix4::perspectiveFovLH(radianFromDegree(60), gfx.getWidth() / static_cast<float>(gfx.getHeight()), 0.01f, 1000);
		//mtxProj = Matrix4::identity;

		_mesh.updateWorldMatrix(mtxWorld.transpose());
		for (auto&& material : _mesh.getMaterials()) {
			material->setParameter<Matrix4>("mtxView", mtxView.transpose());
			material->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
			material->setParameter<Vector3>("direction", Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward));
			material->setParameter<Vector3>("color", color);
			material->setParameter<float>("intensity", intensity);
		}

		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky.updateWorldMatrix(skyMtxWorld.transpose());
		_sky.getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_sky.getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	StaticSingleMeshRender _sky;
	StaticSingleMeshRender _mesh;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	Win32Application app;
	app.init(hInstance, nCmdShow);
	app._sceneManager->changeScene<TestScene_Gun>();

	return app.run();
}