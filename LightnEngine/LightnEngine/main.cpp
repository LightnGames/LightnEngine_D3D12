#include "Win32Application.h"
#include "GraphicsCore.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	GraphicsCore graphicsCore;
	Win32Application app;
	return app.run(hInstance, &graphicsCore, nCmdShow);
}