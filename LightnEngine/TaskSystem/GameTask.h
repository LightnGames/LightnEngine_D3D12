#pragma once

#include <list>
#include <memory>

class GameTask {
public:

	GameTask() {}

	virtual ~GameTask() {}

	virtual void setUpTask() {}

	virtual void onStart() {}

	virtual void onUpdate();

	virtual void onDestroy() {}

	template <typename T, class... _Types>
	RefPtr<T> makeChild(_Types&&... _Args) {
		T* taskPtr = new T(std::forward<_Types>(_Args)...);
		taskPtr->setUpTask();
		_childs.emplace_back(std::unique_ptr<T>(taskPtr));
		return taskPtr;
	}

private:
	std::list<std::unique_ptr<GameTask>> _childs;
};