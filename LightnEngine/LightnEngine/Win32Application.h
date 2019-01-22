#pragma once
#include <Windows.h>
#include "Utility.h"

class GraphicsCore;

class Win32Application {
public:
	Win32Application();
	~Win32Application();

	int run(HINSTANCE hInstance, GraphicsCore* graphicsCore, int nCmdShow);

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	
	//Kariiiiiiiiii
	static GraphicsCore* _tmpCore;

public:
	GETTER(HWND, hwnd);
};

