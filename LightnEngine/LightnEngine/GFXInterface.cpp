#include "GFXInterface.h"
#include <GraphicsCore.h>

GFXInterface* Singleton<GFXInterface>::_singleton = 0;

GFXInterface::~GFXInterface(){
}

void GFXInterface::init(HWND hwnd){
#ifdef D3D12
	_graphicsCore = makeUnique<GraphicsCore>();
	_graphicsCore->onInit(hwnd);
#endif
}

void GFXInterface::onUpdate(){
#ifdef D3D12
	_graphicsCore->onUpdate();
#endif
}

void GFXInterface::onRender(){
#ifdef D3D12
	_graphicsCore->onRender();
#endif
}

void GFXInterface::onDestroy(){
#ifdef D3D12
	_graphicsCore->onDestroy();
#endif
}

void GFXInterface::createTextures(const VectorArray<String>& textureNames){
#ifdef D3D12
	_graphicsCore->createTextures(textureNames);
#endif
}

void GFXInterface::createMeshSets(const VectorArray<String>& fileNames){
#ifdef D3D12
	_graphicsCore->createMeshSets(fileNames);
#endif
}

void GFXInterface::createSharedMaterial(const SharedMaterialCreateSettings& settings){
#ifdef D3D12
	_graphicsCore->createSharedMaterial(settings);
#endif
}
