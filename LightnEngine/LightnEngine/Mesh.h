#pragma once

#include "stdafx.h"
#include "Utility.h"

struct BufferView;
struct SharedMaterial;
class VertexBuffer;
class IndexBuffer;
class RootSignature;
class PipelineState;
class ConstantBuffer;
class VertexShader;
class PixelShader;

#include "GpuResource.h"
#include "CommandContext.h"

using namespace Microsoft::WRL;

class Mesh {
public:
	void create(const std::string& fileName, const std::vector<std::string>& materials) {
	}

	VertexBuffer * _vertexBuffer;
	IndexBuffer* _indexBuffer;

	SharedMaterial* _material;
};