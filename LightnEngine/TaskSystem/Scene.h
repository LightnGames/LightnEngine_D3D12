#pragma once

#include "GameTask.h"

class Scene :public GameTask {
public:
	void onStart() override {
		GameTask::onStart();
	}
	void onUpdate() override {
		GameTask::onUpdate();
	}
	void onDestroy() override {
		GameTask::onDestroy();
	}
};

class SceneManager {
public:

	void changeScene(Scene* newScene) {
		if (_activeScene != nullptr) {
			_activeScene->onDestroy();
			_activeScene = nullptr;
		}
		_activeScene = newScene;
	}

	template<class T>
	T* changeScene() {
		T* newScene = new T();
		newScene->onStart();
		changeScene(newScene);

		return newScene;
	}

	void updateScene() {
		_activeScene->onUpdate();
	}

	Scene * _activeScene;
};