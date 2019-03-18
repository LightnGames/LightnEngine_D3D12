#include "Win32Application.h"
#include "GraphicsCore.h"
#include <dxgidebug.h>

GraphicsCore* Win32Application::_tmpCore = nullptr;

Win32Application::Win32Application() {
}


Win32Application::~Win32Application() {
}

int Win32Application::run(HINSTANCE hInstance, GraphicsCore* graphicsCore, int nCmdShow) {
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = "LightnEngine";
	RegisterClassEx(&windowClass);

	RECT windowRect = { 0, 0, static_cast<LONG>(graphicsCore->width()), static_cast<LONG>(graphicsCore->height()) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	_hwnd = CreateWindow(
		windowClass.lpszClassName,
		"LightnEngine",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		hInstance,
		graphicsCore);
	_tmpCore = graphicsCore;

	graphicsCore->onInit(_hwnd);

	ShowWindow(_hwnd, nCmdShow);

	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	graphicsCore->onDestroy();

#ifdef DEBUG
	//メモリリークしているComオブジェクトを表示
	IDXGIDebug1* pDebug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)))) {
		pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
		pDebug->Release();
	}
#endif

	return static_cast<char>(msg.wParam);
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	GraphicsCore* graphicsCore = reinterpret_cast<GraphicsCore*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	graphicsCore = _tmpCore;

	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
		return true;
	}

	switch (message) {
	case WM_PAINT:
		if (graphicsCore) {
			graphicsCore->onUpdate();
			graphicsCore->onRender();
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
