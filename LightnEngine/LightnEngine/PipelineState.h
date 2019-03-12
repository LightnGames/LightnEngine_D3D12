#pragma once
#include <d3d12.h>
#include <d3dcompiler.h>
#include <vector>
#include <iostream>
#include <locale> 
#include <codecvt> 
#include <cstdio>
#include "stdafx.h"
#include "D3D12Helper.h"

struct ShaderReflectionCBV {
	std::string name;
	std::string type;
	uint32 startOffset;
	uint32 elements;
};

struct ShaderReflectionCB {
	uint32 bindPoint;
	std::string name;
	std::vector<ShaderReflectionCBV> variables;
};

struct ShaderReflectionResult {
	std::vector<ShaderReflectionCB> constantBuffers;
};

struct VertexShader {
	void create(const std::string& fileName) {
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> cv;

		//string→wstring UTF-8 Only No Include Japanese
		std::wstring wFileName = cv.from_bytes(fileName);

		throwIfFailed(D3DCompileFromFile(wFileName.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vertexShader, nullptr));

		inputLayouts = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		byteCode.BytecodeLength = vertexShader->GetBufferSize();
		byteCode.pShaderBytecode = vertexShader->GetBufferPointer();

		ComPtr<ID3D12ShaderReflection> shaderReflector;
		throwIfFailed(D3DReflect(byteCode.pShaderBytecode, byteCode.BytecodeLength, IID_ID3D12ShaderReflection, reinterpret_cast<void**>(shaderReflector.GetAddressOf())));

		D3D12_SHADER_DESC shaderDesc = {};
		shaderReflector->GetDesc(&shaderDesc);

		result.constantBuffers.resize(shaderDesc.ConstantBuffers);

		//定数バッファ取得
		for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i) {
			ID3D12ShaderReflectionConstantBuffer* constantBuffer = shaderReflector->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC constantBufferDesc = {};
			constantBuffer->GetDesc(&constantBufferDesc);

			ShaderReflectionCB shaderReflectionCb;
			shaderReflectionCb.name = constantBufferDesc.Name;
			shaderReflectionCb.variables.resize(constantBufferDesc.Variables);

			for (UINT j = 0; j < constantBufferDesc.Variables; ++j) {
				ID3D12ShaderReflectionVariable* cbVariable = constantBuffer->GetVariableByIndex(j);
				ID3D12ShaderReflectionType* cbVariableType = cbVariable->GetType();

				D3D12_SHADER_VARIABLE_DESC cbVariableDesc = {};
				D3D12_SHADER_TYPE_DESC cbVariableTypeDesc = {};

				cbVariable->GetDesc(&cbVariableDesc);
				cbVariableType->GetDesc(&cbVariableTypeDesc);

				shaderReflectionCb.variables[j].startOffset = cbVariableDesc.StartOffset;
				shaderReflectionCb.variables[j].name = cbVariableDesc.Name;
				shaderReflectionCb.variables[j].type = cbVariableTypeDesc.Name;
				shaderReflectionCb.variables[j].elements = cbVariableTypeDesc.Elements;
			}

			result.constantBuffers[i] = shaderReflectionCb;
		}

		cbvRangeDescs.resize(result.constantBuffers.size());

		//シェーダーリソース取得
		for (int i = 0; i < shaderDesc.BoundResources; ++i) {
			D3D12_SHADER_INPUT_BIND_DESC dd = {};
			shaderReflector->GetResourceBindingDesc(i, &dd);

			switch (dd.Type) {
				//定数バッファならレジスタ番号を調べて登録
			case D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER:
				for (size_t i = 0; i < result.constantBuffers.size(); ++i) {
					auto& cb = result.constantBuffers[i];
					if (cb.name == dd.Name) {
						cb.bindPoint = dd.BindPoint;
					}

					cbvRangeDescs[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
					cbvRangeDescs[i].NumDescriptors = 1;
					cbvRangeDescs[i].BaseShaderRegister = cb.bindPoint;
					cbvRangeDescs[i].RegisterSpace = 0;
					cbvRangeDescs[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
					cbvRangeDescs[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				}
				break;
			}
		}
	}
	
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
	std::vector<D3D12_DESCRIPTOR_RANGE1> cbvRangeDescs;
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayouts;
	ShaderReflectionResult result;
	D3D12_SHADER_BYTECODE byteCode;
};

struct PixelShader {
	void create(const std::string& fileName) {
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> cv;

		//string→wstring UTF-8 Only No Include Japanese
		std::wstring wFileName = cv.from_bytes(fileName);

		throwIfFailed(D3DCompileFromFile(wFileName.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &pixelShader, nullptr));

		byteCode.BytecodeLength = pixelShader->GetBufferSize();
		byteCode.pShaderBytecode = pixelShader->GetBufferPointer();

		ComPtr<ID3D12ShaderReflection> shaderReflector;
		throwIfFailed(D3DReflect(byteCode.pShaderBytecode, byteCode.BytecodeLength, IID_ID3D12ShaderReflection, reinterpret_cast<void**>(shaderReflector.GetAddressOf())));

		D3D12_SHADER_DESC shaderDesc = {};
		shaderReflector->GetDesc(&shaderDesc);

		D3D12_DESCRIPTOR_RANGE1 srvRangeDesc = {};

		//シェーダーリソース取得
		for (int i = 0; i < shaderDesc.BoundResources; ++i) {
			D3D12_SHADER_INPUT_BIND_DESC dd = {};
			shaderReflector->GetResourceBindingDesc(i, &dd);

			switch (dd.Type) {
				//定数バッファならレジスタ番号を調べて登録
			case D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER:
				//for (size_t i = 0; i < result.constantBuffers.size(); ++i) {
				//	auto& cb = result.constantBuffers[i];
				//	if (cb.name == dd.Name) {
				//		cb.bindPoint = dd.BindPoint;
				//	}

				//	cbvRangeDescs[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				//	cbvRangeDescs[i].NumDescriptors = 1;
				//	cbvRangeDescs[i].BaseShaderRegister = cb.bindPoint;
				//	cbvRangeDescs[i].RegisterSpace = 0;
				//	cbvRangeDescs[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
				//	cbvRangeDescs[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				//}
				break;
			case D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE:
				srvRangeDesc.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				srvRangeDesc.NumDescriptors = 1;
				srvRangeDesc.BaseShaderRegister = dd.BindPoint;
				srvRangeDesc.RegisterSpace = 0;
				srvRangeDesc.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
				srvRangeDesc.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				srvRangeDescs.emplace_back(srvRangeDesc);

				break;
			case D3D_SHADER_INPUT_TYPE::D3D_SIT_SAMPLER:
				break;
			}
		int y = 0;
		}

	}

	Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
	std::vector<D3D12_DESCRIPTOR_RANGE1> srvRangeDescs;
	D3D12_SHADER_BYTECODE byteCode;
};

class RootSignature {
public:

	RootSignature() {}

	void create(ID3D12Device* device, const VertexShader& vertexShader, const PixelShader& pixelShader) {
		std::vector<D3D12_ROOT_PARAMETER1> rootParameters;

		//ピクセルシェーダー
		{
			D3D12_ROOT_PARAMETER1 parameterDesc = {};
			parameterDesc.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameterDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			parameterDesc.DescriptorTable.NumDescriptorRanges = pixelShader.srvRangeDescs.size();
			parameterDesc.DescriptorTable.pDescriptorRanges = pixelShader.srvRangeDescs.data();
			rootParameters.emplace_back(parameterDesc);
		}

		//頂点シェーダー
		{
			D3D12_ROOT_PARAMETER1 parameterDesc = {};
			parameterDesc.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameterDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			parameterDesc.DescriptorTable.NumDescriptorRanges = vertexShader.cbvRangeDescs.size();
			parameterDesc.DescriptorTable.pDescriptorRanges = vertexShader.cbvRangeDescs.data();
			rootParameters.emplace_back(parameterDesc);
		}

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSignatureDesc.Desc_1_1.NumParameters = rootParameters.size();
		rootSignatureDesc.Desc_1_1.pParameters = rootParameters.data();
		rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
		rootSignatureDesc.Desc_1_1.pStaticSamplers = &samplerDesc;
		rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		throwIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
		throwIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));
		NAME_D3D12_OBJECT(_rootSignature);
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;
};

class PipelineState {
public:
	void create(ID3D12Device* device, RootSignature* rootSignature, const VertexShader& vertexShader, const PixelShader& pixelShader) {
		D3D12_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.DepthClipEnable = TRUE;
		rasterizerDesc.MultisampleEnable = FALSE;
		rasterizerDesc.AntialiasedLineEnable = FALSE;
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;

		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
			FALSE, FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};

		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
			blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { vertexShader.inputLayouts.data(), static_cast<UINT>(vertexShader.inputLayouts.size()) };
		psoDesc.pRootSignature = rootSignature->_rootSignature.Get();
		psoDesc.VS = vertexShader.byteCode;
		psoDesc.PS = pixelShader.byteCode;
		psoDesc.RasterizerState = rasterizerDesc;
		psoDesc.BlendState = blendDesc;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		throwIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pipelineState)));
		NAME_D3D12_OBJECT(_pipelineState);
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState;
};