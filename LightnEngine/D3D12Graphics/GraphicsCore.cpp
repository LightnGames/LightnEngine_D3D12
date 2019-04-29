#include "GraphicsCore.h"
#include <stdexcept>

#include "D3D12Util.h"
#include "D3D12Helper.h"
#include "SharedMaterial.h"
#include "RenderableEntity.h"

#include "ThirdParty/Imgui/imgui.h"

RefPtr<SharedMaterial> gizmoMat;
D3D12_VERTEX_BUFFER_VIEW gizmoVertex;
ID3D12Resource* gizmoVertexResource;
RootSignature gizmoRootSignature;
PipelineState gizmoPipelineState;

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

	//�R�}���h�L���[����
	_commandContext.create(_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

	//Imgui������
	_imguiWindow.init(hwnd, _device.Get());

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
		throwIfFailed(factory->CreateSwapChainForHwnd(_commandContext.getDirectQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));


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
	_gpuResourceManager.createSharedMaterial(_device.Get(), materialSettings);
	_gpuResourceManager.loadSharedMaterial("GizmoT", &gizmoMat);

	struct GizmoLineVertex {
		Vector3 startPos;
		Vector3 endPos;
		Color color;
	};

	constexpr uint32 MAX_GIZMO = 256;

	D3D12_HEAP_PROPERTIES vertexUploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
	D3D12_RESOURCE_DESC vertexBufferDesc = {};
	vertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexBufferDesc.Width = MAX_GIZMO * sizeof(GizmoLineVertex);
	vertexBufferDesc.Height = 1;
	vertexBufferDesc.DepthOrArraySize = 1;
	vertexBufferDesc.MipLevels = 1;
	vertexBufferDesc.SampleDesc.Count = 1;
	vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	throwIfFailed(_device->CreateCommittedResource(
		&vertexUploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&gizmoVertexResource)
	));

	GizmoLineVertex* mapPtr = nullptr;
	gizmoVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mapPtr));

	new (mapPtr) GizmoLineVertex{ Vector3(0,0,0),Vector3(1,0,0),Color::red };
	mapPtr++;

	new (mapPtr) GizmoLineVertex{ Vector3(0,0.2f,0),Vector3(1,0.2f,0),Color::blue };
	mapPtr++;

	new (mapPtr) GizmoLineVertex{ Vector3(0,0.4f,0),Vector3(1,0.4f,0),Color::green };
	mapPtr++;

	gizmoVertex.BufferLocation = gizmoVertexResource->GetGPUVirtualAddress();
	gizmoVertex.StrideInBytes = sizeof(GizmoLineVertex);
	gizmoVertex.SizeInBytes = static_cast<UINT>(vertexBufferDesc.Width);

	RefPtr<VertexShader> vs;
	RefPtr<PixelShader> ps;
	_gpuResourceManager.loadVertexShader("Shaders/" + materialSettings.vertexShaderName, &vs);
	_gpuResourceManager.loadPixelShader("Shaders/" + materialSettings.pixelShaderName, &ps);

	gizmoRootSignature.create(_device.Get(), *vs, *ps);
	gizmoPipelineState.create(_device.Get(), &gizmoRootSignature, *vs, *ps, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

}

void GraphicsCore::onUpdate() {
	_imguiWindow.startFrame();
}

void GraphicsCore::onRender() {
	auto commandListSet = _commandContext.requestCommandListSet();
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

	
	//�}�e���A���̒萔�o�b�t�@��GPU�փA�b�v���[�h
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

	//gizmoMat->setParameter<Matrix4>("mtxView", mtxView.transpose());
	//gizmoMat->setParameter<Matrix4>("mtxProj", mtxProj.transpose());

	commandList->SetGraphicsRootSignature(gizmoRootSignature._rootSignature.Get());
	commandList->SetPipelineState(gizmoPipelineState._pipelineState.Get());
	commandList->IASetVertexBuffers(0, 1, &gizmoVertex);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->DrawInstanced(2, 3, 0, 0);

	//ImguiWindow�`��
	_imguiWindow.renderFrame(commandList);

	//�`��p���\�[�X�o���A��W�J
	commandList->ResourceBarrier(1, &LTND3D12_RESOURCE_BARRIER::transition(_currentFrameResource->_renderTarget->get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//�R�}���h�L���[�ɃR�}���h���X�g��n���Ď��s
	_commandContext.executeCommandList(commandListSet);
	_commandContext.discardCommandListSet(commandListSet);

	//��ʕ\��
	throwIfFailed(_swapChain->Present(1, 0));

	//�`�抮���܂őҋ@
	moveToNextFrame();
}

void GraphicsCore::onDestroy() {
	//�`�撆�ɔj�����Ă��܂��ƍ���̂ő҂�
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

	gizmoVertexResource->Release();
	gizmoRootSignature._rootSignature = nullptr;
	gizmoPipelineState._pipelineState = nullptr;
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

void GraphicsCore::moveToNextFrame() {
	RefPtr<CommandQueue> commandQueue = _commandContext.getCommandQueue();
	const UINT64 currentFenceValue = _frameResources[_frameIndex]._fenceValue;
	commandQueue->waitForFence(currentFenceValue);

	_frameIndex = _swapChain->GetCurrentBackBufferIndex();
	_currentFrameResource = &_frameResources[_frameIndex];

	const UINT64 fenceValue = commandQueue->incrementFence();
	_frameResources[_frameIndex]._fenceValue = fenceValue;
}
