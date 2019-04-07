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

class SceneManager : Singleton<SceneManager> {
public:

	void changeScene(Scene* newScene);

	template<class T>
	RefPtr<T> changeScene() {
		T* newScene = new T();
		newScene->onStart();
		changeScene(newScene);

		return newScene;
	}

	void updateScene();

	UniquePtr<Scene> _activeScene;
};