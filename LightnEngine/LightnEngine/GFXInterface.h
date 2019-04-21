#pragma once

#include <Windows.h>
#include <Utility.h>
#include <LMath.h>

#define D3D12
class GraphicsCore;
class SharedMaterial;
class Texture2D;
class StaticSingleMeshRender;

class GFXInterface :public Singleton<GFXInterface>{
public:
	~GFXInterface();

	void init(HWND hwnd);
	void onUpdate();
	void onRender();
	void onDestroy();

	void createTextures(const VectorArray<String>& textureNames);
	void createMeshSets(const VectorArray<String>& fileNames);
	void createSharedMaterial(const SharedMaterialCreateSettings& settings);

	void loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial);
	void loadTexture(const String& textureName, RefPtr<Texture2D>& dstTexture);

	RefPtr<StaticSingleMeshRender> createStaticSingleMeshRender(const String& name, const VectorArray<String>& materialNames);

	uint32 getWidth() const;
	uint32 getHeight() const;

private:
#ifdef D3D12
	UniquePtr<GraphicsCore> _graphicsCore;
#endif
};