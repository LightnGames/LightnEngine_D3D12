#include "GraphicsCore.h"
#include <stdexcept>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "SharedMaterial.h"
#include "MeshRender.h"

#include "ThirdParty/Imgui/imgui.h"

#include <fstream>

StaticMultiMeshRCG multiRCG;

bool gpuDrivenStenby = false;

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
		debugControllerGpu->SetEnableGPUBasedValidation(true);
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

	//GPU�R�}���h�i�[�A���P�[�^������
	_gpuCommandArray.init(163840);

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

	VectorArray<D3D12_INPUT_ELEMENT_DESC> inputLayouts = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "MATRIX",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	{ "MATRIX",         1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	{ "MATRIX",         2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	{ "MATRIX",         3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	{ "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	};

	String fullPath = "Resources/Environment/meshes.scene";
	std::ifstream fin(fullPath.c_str(), std::ios::in | std::ios::binary);
	fin.exceptions(std::ios::badbit);

	assert(!fin.fail() && "���b�V���t�@�C�����ǂݍ��߂܂���");

	char materialName[64];
	uint32 textureCount;
	uint32 meshCount;

	fin.read(reinterpret_cast<char*>(materialName), 64);
	fin.read(reinterpret_cast<char*>(&textureCount), 4);
	fin.read(reinterpret_cast<char*>(&meshCount), 4);

	VectorArray<String> textureNames(textureCount);
	for (uint32 i = 0; i < textureCount; ++i) {
		char textureName[64];
		fin.read(reinterpret_cast<char*>(textureName), 64);

		textureNames[i] = String("Environment/").append(String(textureName)).append(".dds");
	}

	VectorArray<String> meshNames(meshCount);
	VectorArray<PerMeshData> meshDatas(meshCount);
	for (uint32 i = 0; i < meshCount; ++i) {
		uint32 subMeshCount;
		uint32 instanceCount;
		char meshName[64];
		fin.read(reinterpret_cast<char*>(meshName), 64);
		fin.read(reinterpret_cast<char*>(&instanceCount), 4);
		fin.read(reinterpret_cast<char*>(&subMeshCount), 4);

		meshNames[i] = String("Environment/").append(String(meshName)).append(".mesh");

		PerMeshData& meshData = meshDatas[i];
		meshData.perInstanceVertex.resize(instanceCount);
		meshData.textureIndices.resize(subMeshCount);

		for (uint32 j = 0; j < subMeshCount; ++j) {
			fin.read(reinterpret_cast<char*>(&meshData.textureIndices[j]), 16);
		}

		for (uint32 j = 0; j < instanceCount; ++j) {
			Vector3 position;
			Quaternion rotation;
			Vector3 scale;
			fin.read(reinterpret_cast<char*>(&position), 12);
			fin.read(reinterpret_cast<char*>(&rotation), 16);
			fin.read(reinterpret_cast<char*>(&scale), 12);

			meshData.perInstanceVertex[j].mtxWorld = Matrix4::createWorldMatrix(position, rotation, scale).transpose();
			meshData.perInstanceVertex[j].startPosAABB = position;
			meshData.perInstanceVertex[j].color = Color(j * 0.02f, 0, 0, 1);
			meshData.perInstanceVertex[j].indirectArgumentIndex = static_cast<uint32>(i);
		}
	}

	fin.close();

	VectorArray<IndirectMeshInfo> indirectMeshes(meshCount);

	_gpuResourceManager.createVertexAndIndexBuffer(_device.Get(), _graphicsCommandContext, meshNames);

	for (size_t i = 0; i < indirectMeshes.size(); ++i) {
		RefPtr<VertexAndIndexBuffer> viBuffer;
		_gpuResourceManager.loadVertexAndIndexBuffer(meshNames[i], &viBuffer);
		indirectMeshes[i].vertexAndIndexBuffer = viBuffer;
		indirectMeshes[i].maxInstanceCount = static_cast<uint32>(meshDatas[i].perInstanceVertex.size());

		const uint32 instanceCount = indirectMeshes[i].maxInstanceCount;
		VectorArray<ObjectInfo> perInstance(instanceCount);

		//�V�[���ɔz�u����Ă���I�u�W�F�N�g�̃��[���h�s����}�b�v
		for (size_t j = 0; j < perInstance.size(); ++j) {
			perInstance[j].mtxWorld = meshDatas[i].perInstanceVertex[j].mtxWorld;
			perInstance[j].startPosAABB = meshDatas[i].perInstanceVertex[j].startPosAABB;
			perInstance[j].color = Color(j * 0.02f, 0, 0, 1);
			perInstance[j].indirectArgumentIndex = static_cast<uint32>(i);
		}

		indirectMeshes[i].matrices = perInstance;
		indirectMeshes[i].textureIndices = meshDatas[i].textureIndices;
	}

	_gpuResourceManager.createTextures(_device.Get(), _graphicsCommandContext, textureNames);

	VectorArray<RefPtr<ID3D12Resource>> ppTextureResources(textureNames.size());
	for (size_t i = 0; i < textureNames.size(); ++i) {
		RefPtr<Texture2D> texture;
		_gpuResourceManager.loadTexture(textureNames[i], &texture);
		ppTextureResources[i] = texture->get();
	}

	_descriptorHeapManager.createTextureShaderResourceView(ppTextureResources.data(), &multiRCG.srv, textureCount);

	multiRCG.create(_device.Get(), &_graphicsCommandContext, indirectMeshes, "TestI");

	gpuDrivenStenby = true;
}

void GraphicsCore::onUpdate() {
	_imguiWindow.startFrame();

	static Vector3 positionC = -Vector3::forward * 30 + Vector3::up * 10;
	static float pitchC = 0;
	static float yawC = 0;
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

	static Vector3 positionV = -Vector3::forward * 30;
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

	if (gpuDrivenStenby) {
		multiRCG.updateCullingCameraInfo(virtualCamera, _frameIndex);
	}
}

void GraphicsCore::onRender() {
	if (gpuDrivenStenby) {
		multiRCG.onCompute(&_graphicsCommandContext, _frameIndex);
	}

	auto commandListSet = _graphicsCommandContext.requestCommandListSet();
	RefPtr<ID3D12GraphicsCommandList> commandList = commandListSet.commandList;

	//�f�X�N���v�^�q�[�v���Z�b�g
	RefPtr<ID3D12DescriptorHeap> ppHeap[] = { _descriptorHeapManager.getD3dDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
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
	commandList->ClearDepthStencilView(_dsv.cpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//�}�e���A���̒萔�o�b�t�@��GPU�փA�b�v���[�h
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

	//���b�V����`��
	RenderSettings renderSettings(commandList, _frameIndex);

	union GpuCommandGroup {
		RefPtr<StaticSingleMeshRCG> rcg;
		byte* bytePtr;
	};

	GpuCommandGroup group;
	//���j�A�A���P�[�^�[�ɘA�����ĕ`��R�}���h�����݂���̂�
	//�|�C���^��C�ӂŃI�t�Z�b�g���Ȃ��烌���_�[�R�}���h�O���[�v�̃f�[�^���Q�Ƃ��ă��[�v����
	group.rcg = reinterpret_cast<StaticSingleMeshRCG*>(_gpuCommandArray.mainMemory);
	for (uint32 i = 0; i < _gpuCommandCount; ++i) {
		group.rcg->setupRenderCommand(renderSettings);
		group.bytePtr += group.rcg->getRequireMemorySize();
	}

	//�f�o�b�O�`��R�}���h�����@1�t���[�����Ƃɕ`�惊�X�g�̓N���[���A�b�v�����
	_debugGeometryRender.updatePerInstanceData(_frameIndex);
	_debugGeometryRender.setupRenderCommand(renderSettings);
	_debugGeometryRender.clearDebugDatas();

	CameraConstantRaw cr;
	cr.mtxView = mainCamera->getViewMatrixTransposed();
	cr.mtxProj = mainCamera->getProjectionMatrixTransposed();
	cr.cameraPosition = mainCamera->getPosition();

	multiRCG.cb.writeBufferData(&cr, sizeof(CameraConstantRaw), 0);
	multiRCG.cb.flashBufferData(_frameIndex);

	multiRCG.setupRenderCommand(renderSettings);

	//ImguiWindow�`��
	_imguiWindow.renderFrame(commandList);

	//�`��p���\�[�X�o���A��W�J
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//�R�}���h�L���[�ɃR�}���h���X�g��n���Ď��s
	_graphicsCommandContext.executeCommandList(commandListSet);
	_graphicsCommandContext.discardCommandListSet(commandListSet);

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

	multiRCG.destroy();

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
	//���b�V���C���X�^���X����
	RefPtr<VertexAndIndexBuffer> buffers;
	_gpuResourceManager.loadVertexAndIndexBuffer(name, &buffers);

	const size_t materialCount = buffers->materialDrawRanges.size();
	VectorArray<MaterialSlot> slots;
	VectorArray<RefPtr<SharedMaterial>> materials;
	materials.reserve(materialCount);
	slots.reserve(materialCount);

	//�}�e���A���X���b�g�Ƀ}�e���A�����Z�b�g
	for (size_t i = 0; i < materialNames.size(); ++i) {
		RefPtr<SharedMaterial> material;
		_gpuResourceManager.loadSharedMaterial(materialNames[i], &material);

		slots.emplace_back(buffers->materialDrawRanges[i], material->getRefSharedMaterial());
		materials.emplace_back(material);
	}

	_gpuCommandCount++;

	const size_t memorySize = StaticSingleMeshRCG::getRequireMemorySize(slots.size());
	byte* r = _gpuCommandArray.divideMemory(memorySize);

	//�A���P�[�g�����������̐擪����RCG�C���X�^���X�𐶐�
	RefPtr<StaticSingleMeshRCG> render = new (r) StaticSingleMeshRCG(
		buffers->vertexBuffer.getRefVertexBufferView(),
		buffers->indexBuffer.getRefIndexBufferView(),
		slots);

	return StaticSingleMeshRender(materials, render);
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
