#include "Win32Application.h"
#include "GFXInterface.h"
#include <GraphicsCore.h>
#include <Scene.h>
#include <dxgidebug.h>

UniquePtr<GFXInterface> Win32Application::_tmpCore = nullptr;
UniquePtr<SceneManager> Win32Application::_sceneManager = nullptr;

Win32Application::Win32Application() :_hwnd(0){
}


Win32Application::~Win32Application() {
}

void Win32Application::init(HINSTANCE hInstance, int nCmdShow){
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = "LightnEngine";
	RegisterClassEx(&windowClass);

	RECT windowRect = { 0, 0, static_cast<LONG>(1280), static_cast<LONG>(720) };
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
		nullptr);

	_sceneManager = makeUnique<SceneManager>();
	_tmpCore = makeUnique<GFXInterface>();
	_tmpCore->init(_hwnd);

	ShowWindow(_hwnd, nCmdShow);
}

int Win32Application::run() {
	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	_tmpCore->onDestroy();
	_tmpCore.reset();

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
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
		return true;
	}

	switch (message) {
	case WM_PAINT:
		if (_tmpCore) {
			_tmpCore->onUpdate();
			_sceneManager->updateScene();
			_tmpCore->onRender();
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
