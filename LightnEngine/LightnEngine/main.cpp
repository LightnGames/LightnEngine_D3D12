#include "Win32Application.h"
#include "GFXInterface.h"
//#include <MeshRenderSet.h>
#include <Scene.h>

class TestScene :public Scene {
public:
	void onStart() override {
		Scene::onStart();
		return;

		GFXInterface& gfx = GFXInterface::instance();

		String meshName("Cerberus/Cerberus.fbx");
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
		materialSettings.vertexShaderName = "shaders.hlsl";
		materialSettings.pixelShaderName = "shaders.hlsl";
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

		////メッシュデータ読み込み
		//gfx.createMeshSets({ meshName,"skySphere.fbx" });
		////_gpuResourceManager->loadMeshSets(meshName, _mesh);
		////_gpuResourceManager->loadMeshSets("skySphere.fbx", _sky);

		////MaterialSlot slot;
		////slot.indexCount = _mesh->_indexBuffer->_indexCount;
		////slot.indexOffset = 0;
		////_gpuResourceManager->loadSharedMaterial("TestM", slot.material);
		////_mesh->_materialSlots.emplace_back(slot);

		////MaterialSlot skySlot;
		////skySlot.indexCount = _sky->_indexBuffer->_indexCount;
		////skySlot.indexOffset = 0;
		////_gpuResourceManager->loadSharedMaterial("TestS", skySlot.material);
		////_sky->_materialSlots.emplace_back(skySlot);
	}
	void onUpdate() override {
		Scene::onUpdate();
	}
	void onDestroy() override {
		Scene::onDestroy();
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	Win32Application app;
	app.init(hInstance, nCmdShow);
	app._sceneManager->changeScene<TestScene>();

	return app.run();
}