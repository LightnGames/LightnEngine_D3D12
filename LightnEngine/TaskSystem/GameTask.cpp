#include "GameTask.h"

void GameTask::onUpdate(){
	for (auto& childTask : _childs) {
		childTask->onUpdate();
	}
}
