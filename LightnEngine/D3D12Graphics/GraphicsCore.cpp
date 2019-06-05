#include "GraphicsCore.h"
#include <stdexcept>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "SharedMaterial.h"

#include "ThirdParty/Imgui/imgui.h"

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

		//���zDirect3D12�Ή�GPU�͖�������
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		//�f�o�C�X�����̊m�F������������I��
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
			break;
		}
	}

	//�f�o�C�X����
	throwIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device)));

	//�f�X�N���v�^�q�[�v�}�l�[�W���[
	_descriptorHeapManager.create(_device.Get());

	//�R�}���h�R���e�L�X�g����
	_graphicsCommandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
	_computeCommandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);

	//Imgui������
	_imguiWindow.init(hwnd, _device.Get());

	//�f�o�b�O�`��@�\������
	_debugGeometryRender.create(_device.Get(), &_graphicsCommandContext);

	//�X���b�v�`�F�[������
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


		//�t���X�N���[���T�|�[�g���I�t
		throwIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		throwIfFailed(swapChain.As(&_swapChain));
	}

	//�r���[�|�[�g������
	_viewPort.TopLeftX = 0.0f;
	_viewPort.TopLeftY = 0.0f;
	_viewPort.Width = static_cast<float>(_width);
	_viewPort.Height = static_cast<float>(_height);
	_viewPort.MinDepth = 0.0f;
	_viewPort.MaxDepth = 1.0f;

	_scissorRect = { 0, 0, static_cast<LONG>(_width), static_cast<LONG>(_height) };

	//�f�v�X�o�b�t�@����
	{
		DepthTextureInfo depthInfo;
		depthInfo.width = _width;
		depthInfo.height = _height;
		depthInfo.format = DepthStencilFormat;
		_depthStencil.createDepth(_device.Get(), depthInfo);

		_descriptorHeapManager.createDepthStencilView(_depthStencil.getAdressOf(), &_dsv, 1);
	}

	//�t���[�����\�[�X
	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].create(_device.Get(), _swapChain.Get(), i);
	}

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];

	_mainCameraConstantBuffer.create(_device.Get(), { sizeof(CameraConstantBuffer) });
	_directionalLightBuffer.create(_device.Get(), { sizeof(DirectionalLightConstantBuffer) });
	_pointlLightBuffer.create(_device.Get(), { sizeof(PointLightConstantBuffer) });

	String diffuseEnv("cubemapEnvHDR.dds");
	createTextures({ diffuseEnv });

	InitSettingsPerSingleMesh singleMeshMaterialInfo = {};
	singleMeshMaterialInfo.vertexShaderName = "Shaders/skyShaders.hlsl";
	singleMeshMaterialInfo.pixelShaderName = "Shaders/skyShaders.hlsl";
	singleMeshMaterialInfo.textureNames = { diffuseEnv };

	String skyName("skySphere.mesh");
	createMeshSets({ skyName });

	SingleMeshRenderInstance instance = createSingleMeshRenderInstance(skyName, { singleMeshMaterialInfo });
	instance.updateWorldMatrix(Matrix4::scaleXYZ(Vector3::one * 100));
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

	CameraConstantBuffer cr = mainCamera->getCameraConstantBuffer();
	_mainCameraConstantBuffer.writeBufferData(&cr, sizeof(CameraConstantBuffer));
	_mainCameraConstantBuffer.flashBufferData(_frameIndex);

	static float pitchL = 1.0f;
	static float yawL = 0.2f;
	static float rollL = 0;
	static Vector3 color = Vector3::one;
	static float intensity = 1.0f;

	ImGui::Begin("DirectionalLight");
	ImGui::SliderAngle("Picth", &pitchL);
	ImGui::SliderAngle("Yaw", &yawL);
	ImGui::SliderAngle("Roll", &rollL);
	ImGui::SliderFloat("Intensity", &intensity, 0, 10);
	ImGui::ColorEdit3("Color", (float*)& color);
	ImGui::End();

	DirectionalLightConstantBuffer directionalLight;
	directionalLight.color = Color(color.x, color.y, color.z, 1);
	directionalLight.intensity = intensity;
	directionalLight.direction = Quaternion::rotVector(Quaternion::euler({ pitchL, yawL, rollL }, true), Vector3::forward);

	PointLightConstantBuffer pointLight;
	pointLight.position = Vector3::zero;
	pointLight.color = Color::green;
	pointLight.attenuation = Vector3::zero;

	_directionalLightBuffer.writeBufferData(&directionalLight, sizeof(directionalLight));
	_directionalLightBuffer.flashBufferData(_frameIndex);

	_pointlLightBuffer.writeBufferData(&pointLight, sizeof(pointLight));
	_pointlLightBuffer.flashBufferData(_frameIndex);
}

void GraphicsCore::onRender() {
	D3D12_GPU_VIRTUAL_ADDRESS cameraBufferAddress = _mainCameraConstantBuffer.constantBuffers[_frameIndex].getGpuVirtualAddress();
	RefPtr<ID3D12DescriptorHeap> ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
	
	//GPU�J�����O
	{
		auto commandListSet = _graphicsCommandContext.requestCommandListSet();
		RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

		//�f�X�N���v�^�q�[�v���Z�b�g
		commandList->SetDescriptorHeaps(1, ppHeap);

		//�`��ݒ�
		RenderSettings renderSettings(commandList, cameraBufferAddress, _frameIndex);
		for (auto&& multiMesh : _multiMeshes) {
			multiMesh.onCompute(renderSettings);
		}

		//�R�}���h�L���[�ɃR�}���h���X�g��n���Ď��s
		_graphicsCommandContext.executeCommandList(commandListSet);
		_graphicsCommandContext.discardCommandListSet(commandListSet);
		_graphicsCommandContext.waitForIdle();
	}

	//�f�v�X�v���p�X
	{
		auto commandListSet = _graphicsCommandContext.requestCommandListSet();
		RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

		//�f�X�N���v�^�q�[�v���Z�b�g
		commandList->SetDescriptorHeaps(1, ppHeap);

		//�r���[�|�[�g�ݒ�
		commandList->RSSetViewports(1, &_viewPort);
		commandList->RSSetScissorRects(1, &_scissorRect);

		//�f�v�X�p�X�Ȃ̂Ńf�v�X�o�b�t�@�̂݃o�C���h
		commandList->OMSetRenderTargets(0, nullptr, FALSE, &_dsv.cpuHandle);

		//�f�v�X�o�b�t�@�N���A
		commandList->ClearDepthStencilView(_dsv.cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		//�f�v�X�p�X��`��
		RenderSettings renderSettings(commandList, cameraBufferAddress, _frameIndex);
		for (auto&& mesh : _singleMeshes) {
			mesh.setupDepthPassCommand(renderSettings);
		}

		for (auto&& mesh : _multiMeshes) {
			mesh.setupDepthPassCommand(renderSettings);
		}

		_graphicsCommandContext.executeCommandList(commandListSet);
		_graphicsCommandContext.discardCommandListSet(commandListSet);
		_graphicsCommandContext.waitForIdle();
	}

	//���C���p�X
	{
		auto commandListSet = _graphicsCommandContext.requestCommandListSet();
		RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

		//�f�X�N���v�^�q�[�v���Z�b�g
		commandList->SetDescriptorHeaps(1, ppHeap);

		//�r���[�|�[�g�ݒ�
		commandList->RSSetViewports(1, &_viewPort);
		commandList->RSSetScissorRects(1, &_scissorRect);

		//�R�}���h�ςޗp�̃��\�[�X�o���A��W�J
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		//�����_�[�^�[�Q�b�g�E�f�v�X�X�e���V���o�b�t�@���Z�b�g
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _currentFrameResource->_rtv.cpuHandle;
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &_dsv.cpuHandle);

		//�����_�[�^�[�Q�b�g�N���A
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		//���C���p�X�`��
		RenderSettings renderSettings(commandList, cameraBufferAddress, _frameIndex);
		for (auto&& mesh : _singleMeshes) {
			mesh.setupMainPassCommand(renderSettings);
		}

		for (auto&& mesh : _multiMeshes) {
			mesh.setupMainPassCommand(renderSettings);
		}

		//�f�o�b�O�`��R�}���h�����@1�t���[�����Ƃɕ`�惊�X�g�̓N���[���A�b�v�����
		_debugGeometryRender.updatePerInstanceData(_frameIndex);
		_debugGeometryRender.setupRenderCommand(renderSettings);
		_debugGeometryRender.clearDebugDatas();

		//ImguiWindow�`��
		_imguiWindow.renderFrame(commandList);

		//�`��p���\�[�X�o���A��W�J
		commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		//�R�}���h�L���[�ɃR�}���h���X�g��n���Ď��s
		_graphicsCommandContext.executeCommandList(commandListSet);
		_graphicsCommandContext.discardCommandListSet(commandListSet); 
	}

	//��ʕ\��
	throwIfFailed(_swapChain->Present(1, 0));

	//�`�抮���܂őҋ@
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//�`�撆�ɔj�����Ă��܂��ƍ���̂ő҂�
	_graphicsCommandContext.waitForIdle();

	for (int i = 0; i < FrameCount; ++i) {
		_frameResources[i].shutdown();
	}

	_mainCameraConstantBuffer.shutdown();
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

void GraphicsCore::createSingleMeshMaterial(const String& name, const InitSettingsPerSingleMesh& singleMeshMaterialInfo){
	//auto itr = _singleMeshRenderMaterials.emplace(std::piecewise_construct,
	//	std::make_tuple(name),
	//	std::make_tuple());

	//RefPtr<SingleMeshRenderMaterial> material = &(*itr.first).second;
	//material->create(_device.Get(), &_graphicsCommandContext, singleMeshMaterialInfo);
}

SingleMeshRenderInstance GraphicsCore::createSingleMeshRenderInstance(const String& name, const VectorArray<InitSettingsPerSingleMesh>& materialInfos){
	StaticSingleMesh singleMesh;
	singleMesh.create(_device.Get(), name, _mainCameraConstantBuffer, materialInfos);

	_singleMeshes.emplace_back(std::move(singleMesh));

	SingleMeshRenderInstance instance;
	instance._mesh = &_singleMeshes.back();
	return instance;
}

StaticMultiMeshRenderInstance GraphicsCore::createStaticMultiMeshRender(const String& name, const InitSettingsPerStaticMultiMesh& meshDatas){
	//auto itr = _multiMeshRenderMaterials.emplace(std::piecewise_construct,
	//	std::make_tuple(name),
	//	std::make_tuple());

	//RefPtr<StaticMultiMeshMaterial> material = &(*itr.first).second;
	//material->create(_device.Get(), &_graphicsCommandContext, meshDatas);

	InitBufferInfo bufferInfo = { _mainCameraConstantBuffer,_directionalLightBuffer,_pointlLightBuffer };

	StaticMultiMesh multiMesh;
	multiMesh.create(_device.Get(), &_graphicsCommandContext, bufferInfo, meshDatas);
	_multiMeshes.emplace_back(std::move(multiMesh));

	//return StaticMultiMeshRenderInstance(material);
	return StaticMultiMeshRenderInstance(nullptr);
}

RefPtr<GpuResourceManager> GraphicsCore::getGpuResourceManager() {
	return &_gpuResourceManager;
}

RefPtr<DebugGeometryRender> GraphicsCore::getDebugGeometryRender() {
	return &_debugGeometryRender;
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
