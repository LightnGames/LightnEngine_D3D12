#pragma once

#include <Windows.h>
#include <Utility.h>

#define D3D12
class GraphicsCore;

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

private:
	#ifdef D3D12
	UniquePtr<GraphicsCore> _graphicsCore;
	#endif
};