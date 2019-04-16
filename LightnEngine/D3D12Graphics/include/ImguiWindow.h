#pragma once

#include "Utility.h"
#include <Windows.h>

struct BufferView;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class ImguiWindow {
public:
	~ImguiWindow();

	//ImguiWIndow初期化
	void init(HWND& hwnd, ID3D12Device* device);

	//Imgui描画初期化処理
	void startFrame();

	//ImguiWIndowを描画
	void renderFrame(ID3D12GraphicsCommandList* commandList);

	//Imgui破棄
	void shutdown();

private:
	RefPtr<BufferView> _imguiView;
};