#pragma once
#include <Windows.h>
#include <Utility.h>

class GFXInterface;
class SceneManager;

class Win32Application {
public:
	Win32Application();
	~Win32Application();

	void init(HINSTANCE hInstance, int nCmdShow);
	int run();

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	

public:
	HWND _hwnd;

	static UniquePtr<GFXInterface> _tmpCore;
	static UniquePtr<SceneManager> _sceneManager;
};
