#include "GraphicsCore.h"
#include <stdexcept>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "SharedMaterial.h"
#include "MeshRender.h"
#include "StaticMultiMesh.h"

#include "ThirdParty/Imgui/imgui.h"

RootSignature depthPrePassSignature;
PipelineState depthPrePassState;
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
	ComPtr<ID3D12Debug1> debugControllerGpu;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		debugController->QueryInterface(IID_PPV_ARGS(&debugControllerGpu));
		//debugControllerGpu->SetEnableGPUBasedValidation(true);
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

	//コマンドコンテキスト生成
	_graphicsCommandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
	_computeCommandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);

	//Imgui初期化
	_imguiWindow.init(hwnd, _device.Get());

	//デバッグ描画機能初期化
	_debugGeometryRender.create(_device.Get(), &_graphicsCommandContext);

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
		throwIfFailed(factory->CreateSwapChainForHwnd(_graphicsCommandContext.getDirectQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));


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

	return;
	DefaultPipelineStateDescSet psoDescSet = {};

	VertexShader vs;
	PixelShader ps;
	depthPrePassSignature.create(_device.Get(), vs, ps);
	depthPrePassState.create(_device.Get(), &depthPrePassSignature, vs, ps, psoDescSet);
}

void GraphicsCore::onUpdate() {
	_imguiWindow.startFrame();

	static Vector3 positionC = -Vector3::forward * 15 + Vector3::up * 2.5f + Vector3::right * -10;
	static float pitchC = -0.2f;
	static float yawC = 1.0f;
	static float rollC = 0;
	static float fov = 60;
	static float farZ = 1000;
	static float nearZ = 0.01f;

	ImGui::Begin("Camera");
	ImGui::DragFloat3("Position", (float*)& positionC, 0.05f);
	ImGui::SliderAngle("Picth", &pitchC);
	ImGui::SliderAngle("Yaw", &yawC);
	ImGui::SliderAngle("Roll", &rollC);
	ImGui::SliderFloat("Fov", &fov, 0, 120);
	ImGui::SliderFloat("NearZ", &nearZ, 0.001f, 10);
	ImGui::SliderFloat("FarZ", &farZ, 10, 1000);
	ImGui::End();

	RefPtr<Camera> mainCamera = _gpuResourceManager.getMainCamera();
	mainCamera->setPosition(positionC);
	mainCamera->setRotationEuler(pitchC, yawC, rollC, true);
	mainCamera->setFieldOfView(fov);
	mainCamera->setNearZ(nearZ);
	mainCamera->setFarZ(farZ);
	mainCamera->setAspectRate(_width, _height);
	mainCamera->computeProjectionMatrix();
	mainCamera->computeViewMatrix();
	mainCamera->computeFlustomNormals();

	static Vector3 positionV = -Vector3::forward * 23 + Vector3::up * 5;
	static float pitchV = 0;
	static float yawV = 0;
	static float rollV = 0;
	static float fovV = 60;
	static float farZV = 10;
	static float nearZV = 0.5f;

	ImGui::Begin("Virtual Camera");
	ImGui::DragFloat3("Position", (float*)& positionV, 0.05f);
	ImGui::SliderAngle("Picth", &pitchV);
	ImGui::SliderAngle("Yaw", &yawV);
	ImGui::SliderAngle("Roll", &rollV);
	ImGui::SliderFloat("Fov", &fovV, 0, 120);
	ImGui::SliderFloat("NearZ", &nearZV, 0.001f, 10);
	ImGui::SliderFloat("FarZ", &farZV, 10, 1000);
	ImGui::End();

	Camera virtualCamera;
	virtualCamera.setPosition(positionV);
	virtualCamera.setRotationEuler(pitchV, yawV, rollV, true);
	virtualCamera.setFieldOfView(fovV);
	virtualCamera.setNearZ(nearZV);
	virtualCamera.setFarZ(farZV);
	virtualCamera.setAspectRate(_width, _height);
	virtualCamera.computeProjectionMatrix();
	virtualCamera.computeViewMatrix();

	virtualCamera.computeFlustomNormals();
	virtualCamera.debugDrawFlustom();

	for (auto&& multiRcg : _multiRcgs) {
		multiRcg->updateCullingCameraInfo(virtualCamera, _frameIndex);
	}
}

void GraphicsCore::onRender() {
	for (auto&& multiRcg : _multiRcgs) {
		multiRcg->onCompute(&_graphicsCommandContext, _frameIndex);
	}

	auto commandListSet = _graphicsCommandContext.requestCommandListSet();
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

	//メッシュを描画
	RenderSettings renderSettings(commandList, _frameIndex);

	//マテリアルの定数バッファをGPUへアップロード
	RefPtr<Camera> mainCamera = _gpuResourceManager.getMainCamera();
	auto& materials = _gpuResourceManager.getMaterials();
	for (auto& material : materials) {
		SharedMaterial& sharedMaterial = material.second;
		sharedMaterial.setParameter<Matrix4>("mtxWorld", Matrix4::scaleXYZ(Vector3::one * 100).transpose());
		sharedMaterial.setParameter<Matrix4>("mtxView", mainCamera->getViewMatrixTransposed());
		sharedMaterial.setParameter<Matrix4>("mtxProj", mainCamera->getProjectionMatrixTransposed());
		sharedMaterial.setParameter<Vector3>("cameraPos", mainCamera->getPosition());
		sharedMaterial._vertexConstantBuffer.flashBufferData(_frameIndex);
		sharedMaterial._pixelConstantBuffer.flashBufferData(_frameIndex);

		sharedMaterial.setupRenderCommand(renderSettings);
	}

	union GpuCommandGroup {
		RefPtr<StaticSingleMeshRCG> rcg;
		byte* bytePtr;
	};

	GpuCommandGroup group;
	//リニアアロケーターに連続して描画コマンドが存在するので
	//ポインタを任意でオフセットしながらレンダーコマンドグループのデータを参照してループする
	group.rcg = reinterpret_cast<StaticSingleMeshRCG*>(_gpuCommandArray.mainMemory);
	for (uint32 i = 0; i < _gpuCommandCount; ++i) {
		//group.rcg->setupRenderCommand(renderSettings);
		group.bytePtr += group.rcg->getRequireMemorySize();
	}

	CameraConstantRaw cr;
	cr.mtxView = mainCamera->getViewMatrixTransposed();
	cr.mtxProj = mainCamera->getProjectionMatrixTransposed();
	cr.cameraPosition = mainCamera->getPosition();

	for (auto&& multiRcg : _multiRcgs) {
		multiRcg->cb.writeBufferData(&cr, sizeof(CameraConstantRaw), 0);
		multiRcg->cb.flashBufferData(_frameIndex);

		multiRcg->setupRenderCommand(renderSettings);
	}

	//デバッグ描画コマンド発効　1フレームごとに描画リストはクリーンアップされる
	_debugGeometryRender.updatePerInstanceData(_frameIndex);
	_debugGeometryRender.setupRenderCommand(renderSettings);
	_debugGeometryRender.clearDebugDatas();

	//ImguiWindow描画
	_imguiWindow.renderFrame(commandList);

	//描画用リソースバリアを展開
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//コマンドキューにコマンドリストを渡して実行
	_graphicsCommandContext.executeCommandList(commandListSet);
	_graphicsCommandContext.discardCommandListSet(commandListSet);

	//画面表示
	throwIfFailed(_swapChain->Present(1, 0));

	//描画完了まで待機
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//描画中に破棄してしまうと困るので待つ
	_graphicsCommandContext.waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].shutdown();
	}

	for (auto&& multiRcg : _multiRcgs) {
		multiRcg->destroy();
	}

	_gpuCommandArray.shutdown();
	_debugGeometryRender.destroy();

	_imguiWindow.shutdown();
	_gpuResourceManager.shutdown();

	_graphicsCommandContext.shutdown();
	_computeCommandContext.shutdown();
	_descriptorHeapManager.shutdown();

	_swapChain = nullptr;
	_device = nullptr;
}

void GraphicsCore::createTextures(const VectorArray<String>& textureNames) {
	_gpuResourceManager.createTextures(_device.Get(), _graphicsCommandContext, textureNames);
}

void GraphicsCore::createMeshSets(const VectorArray<String>& fileNames) {
	_gpuResourceManager.createVertexAndIndexBuffer(_device.Get(), _graphicsCommandContext, fileNames);
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

StaticMultiMeshRender GraphicsCore::createStaticMultiMeshRender(const InitSettingsPerStaticMultiMesh& meshDatas){
	StaticMultiMeshRCG* multiRCG = new StaticMultiMeshRCG();
	multiRCG->create(_device.Get(), &_graphicsCommandContext, meshDatas);

	_multiRcgs.emplace_back(multiRCG);

	return StaticMultiMeshRender(multiRCG);
}

RefPtr<GpuResourceManager> GraphicsCore::getGpuResourceManager() {
	return &_gpuResourceManager;
}

RefPtr<DebugGeometryRender> GraphicsCore::getDebugGeometryRender() {
	return &_debugGeometryRender;
}

void GraphicsCore::sky(){
	RefPtr<SharedMaterial> m;
	_gpuResourceManager.loadSharedMaterial("TestS", &m);

	RefPtr<VertexAndIndexBuffer> v;
	_gpuResourceManager.loadVertexAndIndexBuffer("skySphere.mesh", &v);

	VectorArray<RefPtr<Matrix4>> matrics;
	VectorArray<RefPtr<RefVertexAndIndexBuffer>> meshes;

	static Matrix4 ma = Matrix4::scaleXYZ(Vector3::one * 100).transpose();
	matrics.push_back(&ma);

	static RefVertexAndIndexBuffer rv(v->vertexBuffer.getRefVertexBufferView(), v->indexBuffer.getRefIndexBufferView(), v->materialDrawRanges[0]);
	meshes.push_back(&rv);

	m->addMesh(_device.Get(), matrics, meshes);
}

void GraphicsCore::moveToNextFrame() {
	RefPtr<CommandQueue> commandQueue = _graphicsCommandContext.getCommandQueue();
	const UINT64 currentFenceValue = _frameResources[_frameIndex]._fenceValue;
	commandQueue->waitForFence(currentFenceValue);

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];

	const UINT64 fenceValue = commandQueue->incrementFence();
	_frameResources[_frameIndex]._fenceValue = fenceValue;
}
