#pragma once
#include <d3dcompiler.h>
#include <vector>
#include <iostream>
#include <locale> 
#include <codecvt> 
#include <cstdio>
#include <Utility.h>
#include "stdafx.h"
#include "D3D12Helper.h"
#include "GraphicsConstantSettings.h"

struct ShaderReflectionCBV {
	String name;
	String type;
	uint32 startOffset;
	uint32 size;
	uint32 elements;

	//変数の型名からサイズと型名をセット
	void setSizeAndType(const String& name) {
		this->type = name;

		static UnorderedMap<String, uint32> dataTable = {
			{ "float", 4 },
			{ "float2", 8 },
			{ "float3", 12 },
			{ "float4", 16 },
			{ "float4x4", 64 },
			{ "uint", 4 },
			{ "uint2", 8 },
			{ "uint3", 12 },
			{ "uint4", 16 }
		};

		size = dataTable.at(this->type);
	}
};

struct ShaderReflectionCB {
	uint32 bindPoint;
	String name;
	VectorArray<ShaderReflectionCBV> variables;

	//定数バッファに含まれる変数の総サイズを取得
	uint32 getBufferSize() const {
		uint32 size = 0;
		for (const auto& variable : variables) {
			size += variable.size*variable.elements;
		}

		return size;
	}

	//名前から定数バッファに含まれる変数を取得
	const RefPtr<const ShaderReflectionCBV> getVariable(const String& name) const {
		for (const auto& variable : variables) {
			if (variable.name == name) {
				return &variable;
			}
		}

		return nullptr;
	}
};

struct ShaderReflectionResult {
	VectorArray<ShaderReflectionCB> constantBuffers;
	VectorArray<D3D12_DESCRIPTOR_RANGE1> cbvRangeDescs;
	VectorArray<D3D12_DESCRIPTOR_RANGE1> srvRangeDescs;
};

using namespace Microsoft::WRL;

class Shader {
public:
	//シェーダーバイナリからリフレクション結果を取得する
	ShaderReflectionResult getShaderReflection(const D3D12_SHADER_BYTECODE& byteCode) {
		ShaderReflectionResult result;
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
				shaderReflectionCb.variables[j].elements = cbVariableTypeDesc.Elements == 0 ? 1 : cbVariableTypeDesc.Elements;
				shaderReflectionCb.variables[j].setSizeAndType(cbVariableTypeDesc.Name);
			}

			result.constantBuffers[i] = shaderReflectionCb;
		}

		//まずはこのシェーダーに含まれるリソースの数を調べる
		uint32 constantBufferCount = 0;
		uint32 textureCount = 0;
		for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
			D3D12_SHADER_INPUT_BIND_DESC bindDesc = {};
			shaderReflector->GetResourceBindingDesc(i, &bindDesc);

			switch (bindDesc.Type) {
			case D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER:
				constantBufferCount++;
				break;

			case D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE:
				textureCount++;
				break;
			}
		}

		result.cbvRangeDescs.resize(constantBufferCount);
		result.srvRangeDescs.resize(textureCount);

		uint32 constantBufferCounter = 0;
		uint32 textureCounter = 0;

		//調べたリソース数からDescriptorRangeDescを構築
		for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
			D3D12_SHADER_INPUT_BIND_DESC bindDesc = {};
			shaderReflector->GetResourceBindingDesc(i, &bindDesc);

			switch (bindDesc.Type) {
				//定数バッファならレジスタ番号を調べて登録
			case D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER:
				for (size_t j = 0; j < result.constantBuffers.size(); ++j) {
					auto& cb = result.constantBuffers[j];
					if (cb.name == bindDesc.Name) {
						cb.bindPoint = bindDesc.BindPoint;
					}

					result.cbvRangeDescs[j].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
					result.cbvRangeDescs[j].NumDescriptors = 1;
					result.cbvRangeDescs[j].BaseShaderRegister = cb.bindPoint;
					result.cbvRangeDescs[j].RegisterSpace = 0;
					result.cbvRangeDescs[j].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
					result.cbvRangeDescs[j].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				}
				constantBufferCounter++;
				break;

				//テクスチャならSRVとして登録
			case D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE:
				result.srvRangeDescs[textureCounter].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				result.srvRangeDescs[textureCounter].NumDescriptors = 1;
				result.srvRangeDescs[textureCounter].BaseShaderRegister = bindDesc.BindPoint;
				result.srvRangeDescs[textureCounter].RegisterSpace = 0;
				result.srvRangeDescs[textureCounter].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
				result.srvRangeDescs[textureCounter].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				textureCounter++;
				break;
			}
		}

		return result;
	}

	//定数バッファのサイズを定数バッファごとに配列で取得
	VectorArray<uint32> getConstantBufferSizes() const {
		VectorArray<uint32> cbSizes(shaderReflectionResult.constantBuffers.size());
		for (size_t i = 0; i < cbSizes.size(); ++i) {
			cbSizes[i] = shaderReflectionResult.constantBuffers[i].getBufferSize();
		}

		return cbSizes;
	}

	//シェーダーバイナリをD3D12_SHADER_BYTECODEで取得
	D3D12_SHADER_BYTECODE getByteCode() const {
		D3D12_SHADER_BYTECODE byteCode = {};
		byteCode.BytecodeLength = shader->GetBufferSize();
		byteCode.pShaderBytecode = shader->GetBufferPointer();
		return byteCode;
	}

	//シェーダーリソースビューのRootParameterを生成
	D3D12_ROOT_PARAMETER1 getSrvRootParameter(D3D12_SHADER_VISIBILITY visibility) const {
		D3D12_ROOT_PARAMETER1 parameterDesc = {};
		parameterDesc.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc.ShaderVisibility = visibility;
		parameterDesc.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(shaderReflectionResult.srvRangeDescs.size());
		parameterDesc.DescriptorTable.pDescriptorRanges = shaderReflectionResult.srvRangeDescs.data();

		return parameterDesc;
	}

	//定数バッファのRootParameterを生成
	D3D12_ROOT_PARAMETER1 getCbvRootParameter(D3D12_SHADER_VISIBILITY visibility) const {
		D3D12_ROOT_PARAMETER1 parameterDesc = {};
		parameterDesc.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameterDesc.ShaderVisibility = visibility;
		parameterDesc.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(shaderReflectionResult.cbvRangeDescs.size());
		parameterDesc.DescriptorTable.pDescriptorRanges = shaderReflectionResult.cbvRangeDescs.data();

		return parameterDesc;
	}

	Microsoft::WRL::ComPtr<ID3DBlob> shader;
	ShaderReflectionResult shaderReflectionResult;
};

class VertexShader :public Shader {
public:
	void create(const String& fileName) {
		throwIfFailed(D3DCompileFromFile(convertWString(fileName).c_str(), nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &shader, nullptr));

		inputLayouts = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		shaderReflectionResult = getShaderReflection(getByteCode());
	}

	D3D12_ROOT_PARAMETER1 getSrvRootParameter() const {
		return Shader::getSrvRootParameter(D3D12_SHADER_VISIBILITY_VERTEX);
	}

	D3D12_ROOT_PARAMETER1 getCbvRootParameter() const {
		return Shader::getCbvRootParameter(D3D12_SHADER_VISIBILITY_VERTEX);
	}

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayouts;
};

class PixelShader :public Shader {
public:
	void create(const String& fileName) {
		throwIfFailed(D3DCompileFromFile(convertWString(fileName).c_str(), nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &shader, nullptr));
		shaderReflectionResult = getShaderReflection(getByteCode());
	}

	D3D12_ROOT_PARAMETER1 getSrvRootParameter() const {
		return Shader::getSrvRootParameter(D3D12_SHADER_VISIBILITY_PIXEL);
	}

	D3D12_ROOT_PARAMETER1 getCbvRootParameter() const {
		return Shader::getCbvRootParameter(D3D12_SHADER_VISIBILITY_PIXEL);
	}
};

class RootSignature {
public:

	RootSignature() {}

	void create(ID3D12Device* device, const VertexShader& vertexShader, const PixelShader& pixelShader) {
		std::vector<D3D12_ROOT_PARAMETER1> rootParameters;

		//ピクセルシェーダー
		{
			const ShaderReflectionResult& result = pixelShader.shaderReflectionResult;
			if (!result.srvRangeDescs.empty()) {
				rootParameters.emplace_back(pixelShader.getSrvRootParameter());//DescriptorTableIndex: 1
			}

			if (!result.cbvRangeDescs.empty()) {
				rootParameters.emplace_back(pixelShader.getCbvRootParameter());//DescriptorTableIndex: 2
			}
		}

		//頂点シェーダー
		{
			const ShaderReflectionResult& result = vertexShader.shaderReflectionResult;
			if (!result.srvRangeDescs.empty()) {
				rootParameters.emplace_back(vertexShader.getSrvRootParameter());//DescriptorTableIndex: 3
			}

			if (!result.cbvRangeDescs.empty()) {
				rootParameters.emplace_back(vertexShader.getCbvRootParameter());//DescriptorTableIndex: 4
			}
		}

		//とりあえずこの静的サンプラーを使用(現在はこれで固定)
		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		//ルートシグネチャのサポートバージョンをチェック(してるだけ。。。)
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSignatureDesc.Desc_1_1.NumParameters = static_cast<UINT>(rootParameters.size());
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

		D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {};
		defaultRenderTargetBlendDesc.BlendEnable = FALSE;
		defaultRenderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		defaultRenderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		defaultRenderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
		defaultRenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
		defaultRenderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
		defaultRenderTargetBlendDesc.LogicOpEnable = FALSE;
		defaultRenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		defaultRenderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
		defaultRenderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;

		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
			blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
		}

		D3D12_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		dsDesc.StencilEnable = FALSE;
		dsDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		dsDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		D3D12_DEPTH_STENCILOP_DESC dsopDesc = {};
		dsopDesc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		dsopDesc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		dsopDesc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		dsopDesc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		dsDesc.FrontFace = dsopDesc;
		dsDesc.BackFace = dsopDesc;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { vertexShader.inputLayouts.data(), static_cast<UINT>(vertexShader.inputLayouts.size()) };
		psoDesc.pRootSignature = rootSignature->_rootSignature.Get();
		psoDesc.VS = vertexShader.getByteCode();
		psoDesc.PS = pixelShader.getByteCode();
		psoDesc.RasterizerState = rasterizerDesc;
		psoDesc.BlendState = blendDesc;
		psoDesc.DepthStencilState = dsDesc;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = RenderTargetFormat;
		psoDesc.DSVFormat = DepthStencilFormat;
		psoDesc.SampleDesc.Count = 1;
		throwIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pipelineState)));
		NAME_D3D12_OBJECT(_pipelineState);
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState;
};