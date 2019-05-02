struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

struct VSInput {
	float3 position : POSITION;
	float4 color : COLOR;
};

//cbuffer CameraInfo : register(b0)
//{
//	float4x4 mtxView;
//	float4x4 mtxProj;
//	float3 cameraPos;
//}

cbuffer SceneConstantBuffer : register(b0)
{
	float4 velocity;
	float4 offset;
	float4 color;
	float4x4 projection;
};

PSInput VSMain(VSInput input, uint vertexId : SV_VertexID)
{
	PSInput result;

	if (vertexId == 0) {
		input.position = float3(-0.5, -0.5, 0);
	}

	if (vertexId == 1) {
		input.position = float3(-0.0, 0.5, 0);
	}

	if (vertexId == 2) {
		input.position = float3(0.5, -0.5, 0);
	}

	input.position += offset.xyz;

	float4 worldPos = float4(input.position, 1);
	//float4 viewPos = mul(worldPos, mtxView);
	//result.position = mul(viewPos, mtxProj);
	//result.color = input.color;

	result.position = worldPos;
	result.color = color;

	return result;
}

float4 PSMain(PSInput input) : SV_Target
{
	float4 color = input.color;
	return pow(color, 1.0f / 2.2f);
}