#pragma once

#include "Utility.h"

#include <Windows.h>

struct BufferView;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class ImguiWindow {
public:
	~ImguiWindow();

	//ImguiWIndow������
	void init(HWND& hwnd, ID3D12Device* device);

	//Imgui�`�揉��������
	void startFrame();

	//ImguiWIndow��`��
	void renderFrame(ID3D12GraphicsCommandList* commandList);

	//Imgui�j��
	void shutdown();

private:
	RefPtr<BufferView> _imguiView;
};