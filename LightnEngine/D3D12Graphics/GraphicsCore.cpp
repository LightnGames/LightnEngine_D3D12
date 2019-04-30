#include "GraphicsCore.h"
#include <stdexcept>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "SharedMaterial.h"
#include "RenderableEntity.h"

#include "ThirdParty/Imgui/imgui.h"

constexpr uint32 MAX_GIZMO = 256;

RefPtr<SharedMaterial> gizmoMat;
VertexBufferDynamic gizmoVertex;

RefPtr<SharedMaterial> gizmoMatB;
VertexBuffer gizmoBVertexBuffer;
VertexBufferDynamic gizmoVertexB;

GraphicsCore::GraphicsCore() :
	_width(1280),
	_height(720),
	_frameIndex(0),
	_viewPort({}),
	_scissorRect({}),
	_dsv(),
	_currentFrameResource(nullptr) {
}

GraphicsCore::~GraphicsCore() {
}

void GraphicsCore::onInit(HWND hwnd) {
	UINT dxgiFactoryFlags = 0;
#ifdef DEBUG
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif // DEBUG

	ComPtr<IDXGIFactory4> factory;
	throwIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	ComPtr<IDXGIAdapter1> adapter;
	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		//仮想Direct3D12対応GPUは無視する
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		//デバイス生成の確認が成功したら終了
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
			break;
		}
	}

	//デバイス生成
	throwIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device)));

	//デスクリプタヒープマネージャー
	_descriptorHeapManager.create(_device.Get());

	//コマンドキュー生成
	_commandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

	//Imgui初期化
	_imguiWindow.init(hwnd, _device.Get());

	//GPUコマンド格納アロケータ初期化
	_gpuCommandArray.init(163840);

	//スワップチェーン生成
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.Width = _width;
		swapChainDesc.Height = _height;
		swapChainDesc.Format = RenderTargetFormat;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapChain;
		throwIfFailed(factory->CreateSwapChainForHwnd(_commandContext.getDirectQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));


		//フルスクリーンサポートをオフ
		throwIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		throwIfFailed(swapChain.As(&_swapChain));
	}

	//ビューポート初期化
	_viewPort.TopLeftX = 0.0f;
	_viewPort.TopLeftY = 0.0f;
	_viewPort.Width = static_cast<float>(_width);
	_viewPort.Height = static_cast<float>(_height);
	_viewPort.MinDepth = 0.0f;
	_viewPort.MaxDepth = 1.0f;

	_scissorRect = { 0, 0, static_cast<LONG>(_width), static_cast<LONG>(_height) };

	//デプスバッファ生成
	{
		DepthTextureInfo depthInfo;
		depthInfo.width = _width;
		depthInfo.height = _height;
		depthInfo.format = DepthStencilFormat;
		_depthStencil.createDepth(_device.Get(), depthInfo);

		_descriptorHeapManager.createDepthStencilView(_depthStencil.getAdressOf(), &_dsv, 1);
	}

	//フレームリソース
	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].create(_device.Get(), _swapChain.Get(), i);
	}

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];

	{
		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
			{ "POSITION"      , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
			{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};


		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "GizmoB";
		materialSettings.vertexShaderName = "GizmoGeometry.hlsl";
		materialSettings.pixelShaderName = "GizmoGeometry.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = {};
		materialSettings.inputLayouts = inputLayouts;
		materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		_gpuResourceManager.createSharedMaterial(_device.Get(), materialSettings);
		_gpuResourceManager.loadSharedMaterial("GizmoB", &gizmoMatB);

		struct GizmoGeometryVertex {
			Matrix4 mtxWorld;
			Color color;
		};

		gizmoVertexB.createDirectDynamic(_device.Get(), &_commandContext, MAX_GIZMO, sizeof(GizmoGeometryVertex));

		GizmoGeometryVertex* mapPtr = reinterpret_cast<GizmoGeometryVertex*>(gizmoVertexB._dataPtr);
		Vector3 offset = Vector3::forward * 5;

		new (mapPtr) GizmoGeometryVertex { Matrix4::translateXYZ(Vector3(0,0,0) + offset).transpose(), Color::white };
		mapPtr++;

		new (mapPtr) GizmoGeometryVertex { Matrix4::translateXYZ(Vector3(1,0,0) + offset).transpose(), Color::red };
		mapPtr++;

		new (mapPtr) GizmoGeometryVertex { Matrix4::translateXYZ(Vector3(0,0,1) + offset).transpose(), Color::blue };
		mapPtr++;

		new (mapPtr) GizmoGeometryVertex { Matrix4::translateXYZ(Vector3(0,1,0) + offset).transpose(), Color::green };
		mapPtr++;

		VectorArray<Vector3> vertices = {
			{-0.5f,-0.5f,-0.5f},
			{-0.5f, 0.5f,-0.5f},
			{-0.5f, 0.5f,-0.5f},
			{ 0.5f, 0.5f,-0.5f},
			{ 0.5f, 0.5f,-0.5f},
			{ 0.5f,-0.5f,-0.5f},
			{ 0.5f,-0.5f,-0.5f},
			{-0.5f,-0.5f,-0.5f},

			{-0.5f,-0.5f, 0.5f},
			{-0.5f, 0.5f, 0.5f},
			{-0.5f, 0.5f, 0.5f},
			{ 0.5f, 0.5f, 0.5f},
			{ 0.5f, 0.5f, 0.5f},
			{ 0.5f,-0.5f, 0.5f},
			{ 0.5f,-0.5f, 0.5f},
			{-0.5f,-0.5f, 0.5f},

			{-0.5f,-0.5f, 0.5f},
			{-0.5f,-0.5f,-0.5f},
			{-0.5f, 0.5f, 0.5f},
			{-0.5f, 0.5f,-0.5f},
			{ 0.5f, 0.5f, 0.5f},
			{ 0.5f, 0.5f,-0.5f},
			{ 0.5f,-0.5f, 0.5f},
			{ 0.5f,-0.5f,-0.5f},
		};

		gizmoBVertexBuffer.createDirect<Vector3>(_device.Get(), &_commandContext, vertices);
	}

	//デバッグ線生成
	{
		VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
			{ "START_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "END_POSITION"  , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};

		SharedMaterialCreateSettings materialSettings;
		materialSettings.name = "GizmoT";
		materialSettings.vertexShaderName = "GizmoLine.hlsl";
		materialSettings.pixelShaderName = "GizmoLine.hlsl";
		materialSettings.vsTextures = {};
		materialSettings.psTextures = {};
		materialSettings.inputLayouts = inputLayouts;
		materialSettings.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		_gpuResourceManager.createSharedMaterial(_device.Get(), materialSettings);
		_gpuResourceManager.loadSharedMaterial("GizmoT", &gizmoMat);

		struct GizmoLineVertex {
			Vector3 startPos;
			Vector3 endPos;
			Color color;
		};

		gizmoVertex.createDirectDynamic(_device.Get(), &_commandContext, MAX_GIZMO, sizeof(GizmoLineVertex));

		GizmoLineVertex* mapPtr = reinterpret_cast<GizmoLineVertex*>(gizmoVertex._dataPtr);
		Vector3 offset = Vector3::forward * 5;

		new (mapPtr) GizmoLineVertex{ Vector3(0,0,0) + offset,Vector3(1,0,0) + offset,Color::red };
		mapPtr++;

		new (mapPtr) GizmoLineVertex{ Vector3(0,0,0) + offset,Vector3(0,0,1) + offset,Color::blue };
		mapPtr++;

		new (mapPtr) GizmoLineVertex{ Vector3(0,0,0) + offset,Vector3(0,1,0) + offset,Color::green };
		mapPtr++;
	}
}

void GraphicsCore::onUpdate() {
	_imguiWindow.startFrame();

	static float xC = 0.0f;
	static float yC = 0.0f;
	static float zC = 0.0f;
	static float pitchC = 0;
	static float yawC = 0;
	static float rollC = 0;

	ImGui::Begin("Camera");
	ImGui::SliderFloat("World X", &xC, -5, 5);
	ImGui::SliderFloat("World Y", &yC, -5, 5);
	ImGui::SliderFloat("World Z", &zC, -5, 5);
	ImGui::SliderAngle("Picth", &pitchC);
	ImGui::SliderAngle("Yaw", &yawC);
	ImGui::SliderAngle("Roll", &rollC);
	ImGui::End();

	RefPtr<Camera> mainCamera = _gpuResourceManager.getMainCamera();
	mainCamera->setPosition(xC, yC, zC);
	mainCamera->setRotationEuler(pitchC, yawC, rollC, true);
	mainCamera->setFieldOfView(60);
	mainCamera->setNearZ(0.01f);
	mainCamera->setFarZ(1000.0f);
	mainCamera->setAspectRate(_width, _height);
	mainCamera->computeProjectionMatrix();
	mainCamera->computeViewMatrix();
}

void GraphicsCore::onRender() {
	auto commandListSet = _commandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	//デスクリプタヒープをセット
	RefPtr<ID3D12DescriptorHeap> ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	commandList->SetDescriptorHeaps(1, ppHeap);

	//ビューポート設定
	commandList->RSSetViewports(1, &_viewPort);
	commandList->RSSetScissorRects(1, &_scissorRect);

	//コマンド積む用のリソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//レンダーターゲット・デプスステンシルバッファをセット
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _currentFrameResource->_rtv.cpuHandle;
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &_dsv.cpuHandle);

	//レンダーターゲットクリア
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(_dsv.cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	
	//マテリアルの定数バッファをGPUへアップロード
	RefPtr<Camera> mainCamera = _gpuResourceManager.getMainCamera();
	auto& materials = _gpuResourceManager.getMaterials();
	for (auto& material : materials) {
		SharedMaterial& sharedMaterial = material.second;
		sharedMaterial.setParameter<Matrix4>("mtxView", mainCamera->getViewMatrixTransposed());
		sharedMaterial.setParameter<Matrix4>("mtxProj", mainCamera->getProjectionMatrixTransposed());
		sharedMaterial.setParameter<Vector3>("cameraPos", mainCamera->getPosition());
		sharedMaterial._vertexConstantBuffer.flashBufferData(_frameIndex);
		sharedMaterial._pixelConstantBuffer.flashBufferData(_frameIndex);
	}

	//メッシュを描画
	RenderSettings renderSettings(commandList, _frameIndex);

	union GpuCommandGroup {
		RefPtr<StaticSingleMeshRCG> rcg;
		byte* bytePtr;
	};

	GpuCommandGroup group;
	//リニアアロケーターに連続して描画コマンドが存在するので
	//ポインタを任意でオフセットしながらレンダーコマンドグループのデータを参照してループする
	group.rcg = reinterpret_cast<StaticSingleMeshRCG*>(_gpuCommandArray.mainMemory);
	for (uint32 i = 0; i < _gpuCommandCount; ++i) {
		group.rcg->setupRenderCommand(renderSettings);
		group.bytePtr += group.rcg->getRequireMemorySize();
	}

	gizmoMat->setupRenderCommand(renderSettings);
	commandList->IASetVertexBuffers(0, 1, &gizmoVertex._vertexBufferView);
	commandList->DrawInstanced(2, 3, 0, 0);

	gizmoMatB->setupRenderCommand(renderSettings);
	D3D12_VERTEX_BUFFER_VIEW views[] = { gizmoBVertexBuffer._vertexBufferView, gizmoVertexB._vertexBufferView };
	commandList->IASetVertexBuffers(0, 2, views);
	commandList->DrawInstanced(24, 4, 0, 0);

	//ImguiWindow描画
	_imguiWindow.renderFrame(commandList);

	//描画用リソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//コマンドキューにコマンドリストを渡して実行
	_commandContext.executeCommandList(commandListSet);
	_commandContext.discardCommandListSet(commandListSet);

	//画面表示
	throwIfFailed(_swapChain->Present(1, 0));

	//描画完了まで待機
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//描画中に破棄してしまうと困るので待つ
	_commandContext.waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].shutdown();
	}

	_gpuCommandArray.shutdown();

	_imguiWindow.shutdown();
	_gpuResourceManager.shutdown();

	_commandContext.shutdown();
	_descriptorHeapManager.shutdown();

	_swapChain = nullptr;
	_device = nullptr;

	gizmoBVertexBuffer.destroy();
	gizmoVertex.destroy();
	gizmoVertexB.destroy();
}

void GraphicsCore::createTextures(const VectorArray<String>& textureNames) {
	_gpuResourceManager.createTextures(_device.Get(), _commandContext, textureNames);
}

void GraphicsCore::createMeshSets(const VectorArray<String>& fileNames) {
	_gpuResourceManager.createVertexAndIndexBuffer(_device.Get(), _commandContext, fileNames);
}

void GraphicsCore::createSharedMaterial(const SharedMaterialCreateSettings& settings) {
	_gpuResourceManager.createSharedMaterial(_device.Get(), settings);
}

StaticSingleMeshRender GraphicsCore::createStaticSingleMeshRender(const String& name, const VectorArray<String>& materialNames) {
	//メッシュインスタンス生成
	RefPtr<VertexAndIndexBuffer> buffers;
	_gpuResourceManager.loadVertexAndIndexBuffer(name, &buffers);

	const size_t materialCount = buffers->materialDrawRanges.size();
	VectorArray<MaterialSlot> slots;
	VectorArray<RefPtr<SharedMaterial>> materials;
	materials.reserve(materialCount);
	slots.reserve(materialCount);

	//マテリアルスロットにマテリアルをセット
	for (size_t i = 0; i < materialNames.size(); ++i) {
		RefPtr<SharedMaterial> material;
		_gpuResourceManager.loadSharedMaterial(materialNames[i], &material);

		slots.emplace_back(buffers->materialDrawRanges[i], material->getRefSharedMaterial());
		materials.emplace_back(material);
	}

	_gpuCommandCount++;

	const size_t memorySize = StaticSingleMeshRCG::getRequireMemorySize(slots.size());
	byte* r = _gpuCommandArray.divideMemory(memorySize);

	//アロケートしたメモリの先頭からRCGインスタンスを生成
	RefPtr<StaticSingleMeshRCG> render = new (r) StaticSingleMeshRCG(
		buffers->vertexBuffer.getRefVertexBufferView(),
		buffers->indexBuffer.getRefIndexBufferView(),
		slots);

	return StaticSingleMeshRender(materials, render);
}

RefPtr<GpuResourceManager> GraphicsCore::getGpuResourceManager() {
	return &_gpuResourceManager;
}

void GraphicsCore::moveToNextFrame() {
	RefPtr<CommandQueue> commandQueue = _commandContext.getCommandQueue();
	const UINT64 currentFenceValue = _frameResources[_frameIndex]._fenceValue;
	commandQueue->waitForFence(currentFenceValue);

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];

	const UINT64 fenceValue = commandQueue->incrementFence();
	_frameResources[_frameIndex]._fenceValue = fenceValue;
}
