#pragma once

#include "Utility.h"

#include <Windows.h>

struct BufferView;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class ImguiWindow {
public:
	~ImguiWindow();

	//ImguiWIndow‰Šú‰»
	void init(HWND& hwnd, ID3D12Device* device);

	//Imgui•`‰æ‰Šú‰»ˆ—
	void startFrame();

	//ImguiWIndow‚ğ•`‰æ
	void renderFrame(ID3D12GraphicsCommandList* commandList);

	//Imgui”jŠü
	void shutdown();

private:
	RefPtr<BufferView> _imguiView;
};