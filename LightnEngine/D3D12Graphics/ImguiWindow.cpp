#include "ImguiWindow.h"
#include "stdafx.h"
#include "DescriptorHeap.h"
#include "GraphicsConstantSettings.h"

#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_impl_win32.h"
#include "ThirdParty/Imgui/imgui_impl_dx12.h"

ImguiWindow::~ImguiWindow() {
}

void ImguiWindow::init(HWND& hwnd, ID3D12Device* device) {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	DescriptorHeapManager& manager = DescriptorHeapManager::instance();
	_imguiView = manager.getDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->allocateBufferView(1);

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device, FrameCount,
		RenderTargetFormat,
		_imguiView->cpuHandle,
		_imguiView->gpuHandle);
}

void ImguiWindow::startFrame() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImguiWindow::renderFrame(ID3D12GraphicsCommandList * commandList) {
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void ImguiWindow::shutdown() {
	DescriptorHeapManager& manager = DescriptorHeapManager::instance();
	manager.getDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->discardBufferView(_imguiView);

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
