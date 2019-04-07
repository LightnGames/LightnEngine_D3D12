#include "Scene.h"

SceneManager* Singleton<SceneManager>::_singleton = 0;

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