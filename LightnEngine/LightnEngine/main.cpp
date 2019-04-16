#include "Win32Application.h"
#include "GFXInterface.h"
#include <LMath.h>
#include <ThirdParty/Imgui/imgui.h>
#include <ImguiWindow.h>
#include <MeshRenderSet.h>
#include <RenderableEntity.h>
#include <SharedMaterial.h>
#include <GpuResource.h>
#include <Scene.h>

class TestScene :public Scene {
public:
	void onStart() override {
		Scene::onStart();

		GFXInterface& gfx = GFXInterface::instance();

		String meshName("Cerberus/Cerberus.fbx");
		String skyName("skySphere.fbx");
		String diffuseEnv("cubemapEnvHDR.dds");
		String specularEnv("cubemapSpecularHDR.dds");
		String specularBrdf("cubemapBrdf.dds");
		String albedo("Cerberus/Cerberus_A.dds");
		String normal("Cerberus/Cerberus_N.dds");
		String metallic("Cerberus/Cerberus_M.dds");
		String roughness("Cerberus/Cerberus_R.dds");

		//テクスチャ読み込み
		gfx.createTextures({ albedo, normal, metallic, roughness, diffuseEnv, specularEnv, specularBrdf });

		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "TestM";
		materialSettings.vertexShaderName = "shader_vs.hlsl";
		materialSettings.pixelShaderName = "shader_ps.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = { diffuseEnv, specularEnv, specularBrdf, albedo, normal, metallic, roughness };
		gfx.createSharedMaterial(materialSettings);

		SharedMaterialCreateSettings skyMatSettings;
		skyMatSettings.name = "TestS";
		skyMatSettings.vertexShaderName = "skyShaders.hlsl";
		skyMatSettings.pixelShaderName = "skyShaders.hlsl";
		skyMatSettings.vsTextures = {};
		skyMatSettings.psTextures = { diffuseEnv };
		gfx.createSharedMaterial(skyMatSettings);

		//メッシュデータ読み込み
		gfx.createMeshSets({ meshName,skyName });
		_mesh = gfx.createStaticSingleMeshRender(meshName, { "TestM" });
		_mesh2 = gfx.createStaticSingleMeshRender(meshName, { "TestM" });
		_sky = gfx.createStaticSingleMeshRender(skyName, { "TestS" });
	}

	void onUpdate() override {
		Scene::onUpdate();

		static float z = 0.0f;
		static float pitch = 0;
		static float yaw = 0;
		static float roll = 0;

		static float pitchL = 1.0f;
		static float yawL = 0.2f;
		static float rollL = 0;
		static Vector3 color = Vector3::one;
		static float intensity = 1.0f;

		ImGui::Begin("TestD3D12");
		ImGui::Text("Lightn");
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

		Matrix4 mtxWorld = Matrix4::matrixFromQuaternion(Quaternion::euler({ pitch, yaw, roll }, true)).multiply(Matrix4::translateXYZ({ z, 0, 10.5f }));
		Matrix4 mtxView = Matrix4::identity;
		Matrix4 mtxProj = Matrix4::perspectiveFovLH(radianFromDegree(50), gfx.getWidth() / static_cast<float>(gfx.getHeight()), 0.01f, 1000);
		//mtxProj = Matrix4::identity;

		_mesh->updateWorldMatrix(mtxWorld.transpose());
		_mesh->getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_mesh->getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
		_mesh->getMaterial(0)->setParameter<Vector3>("direction", Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward));
		_mesh->getMaterial(0)->setParameter<Vector3>("color", color);
		_mesh->getMaterial(0)->setParameter<float>("intensity", intensity);

		Matrix4 mtxWorld2 = Matrix4::matrixFromQuaternion(Quaternion::euler({ pitch, yaw, roll }, true)).multiply(Matrix4::translateXYZ({ z + 2, 0, 10.5f }));
		_mesh2->updateWorldMatrix(mtxWorld2.transpose());
		
		_mesh2->getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_mesh2->getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
		_mesh2->getMaterial(0)->setParameter<Vector3>("direction", Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward));
		_mesh2->getMaterial(0)->setParameter<Vector3>("color", color);
		_mesh2->getMaterial(0)->setParameter<float>("intensity", intensity);


		Matrix4 skyMtxWorld = Matrix4::scaleXYZ(Vector3::one * 100);
		_sky->updateWorldMatrix(skyMtxWorld.transpose());
		_sky->getMaterial(0)->setParameter<Matrix4>("mtxView", mtxView.transpose());
		_sky->getMaterial(0)->setParameter<Matrix4>("mtxProj", mtxProj.transpose());
	}
	void onDestroy() override {
		Scene::onDestroy();
	}

	RefPtr<StaticSingleMeshRender> _sky;
	RefPtr<StaticSingleMeshRender> _mesh;
	RefPtr<StaticSingleMeshRender> _mesh2;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	Win32Application app;
	app.init(hInstance, nCmdShow);
	app._sceneManager->changeScene<TestScene>();

	return app.run();
}