#include "Scene.h"

SceneManager* Singleton<SceneManager>::_singleton = 0;

SceneManager::SceneManager():_activeScene(nullptr){
}

SceneManager::~SceneManager(){
}

void SceneManager::changeScene(Scene* newScene) {
	if (_activeScene != nullptr) {
		_activeScene->onDestroy();
		_activeScene = nullptr;
	}
	_activeScene = UniquePtr<Scene>(newScene);
}

void SceneManager::updateScene() {
	if (_activeScene != nullptr) {
		_activeScene->onUpdate();
	}
}