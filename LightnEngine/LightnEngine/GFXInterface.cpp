#include "GFXInterface.h"
#include <GraphicsCore.h>
#include <GpuResourceManager.h>
#include <GpuResource.h>
#include <RenderableEntity.h>

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

void GFXInterface::loadSharedMaterial(const String& materialName, RefPtr<SharedMaterial>& dstMaterial){
#ifdef D3D12
	_graphicsCore->getGpuResourceManager()->loadSharedMaterial(materialName, dstMaterial);
#endif
}

void GFXInterface::loadTexture(const String& textureName, RefPtr<Texture2D>& dstTexture){
#ifdef D3D12
	_graphicsCore->getGpuResourceManager()->loadTexture(textureName, dstTexture);
#endif
}

StaticSingleMeshRender GFXInterface::createStaticSingleMeshRender(const String& name, const VectorArray<String>& materialNames) {
	return _graphicsCore->getGpuResourceManager()->createStaticSingleMeshRender(name, materialNames);
}

uint32 GFXInterface::getWidth() const{
#ifdef D3D12
	return _graphicsCore->_width;
#endif
}

uint32 GFXInterface::getHeight() const{
#ifdef D3D12
	return _graphicsCore->_height;
#endif
}
