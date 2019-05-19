struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD;
	float3 viewDir : VIEWDIR;
	float4 color : COLOR;
};

struct VSInput {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
	float4x4 mtxWorld : MATRIX0;
	float4 color : COLOR0;
};

cbuffer CameraInfo : register(b0)
{
	float4x4 mtxView;
	float4x4 mtxProj;
	float3 cameraPos;
}

PSInput VSMain(VSInput input, uint vertexId : SV_VertexID)
{
	PSInput result;

	float4 worldPos = mul(float4(input.position, 1), input.mtxWorld);
	float4 viewPos = mul(worldPos, mtxView);
	result.position = mul(viewPos, mtxProj);
	result.normal = mul(input.normal, (float3x3) input.mtxWorld);
	result.tangent = mul(input.tangent, (float3x3) input.mtxWorld);
	result.binormal = cross(result.normal, result.tangent);

	result.uv = input.uv;
	result.viewDir = normalize(cameraPos - worldPos.xyz);

	result.color = input.color;

	return result;
}

Texture2D t_albedo[] : register(t0);
SamplerState t_sampler : register(s0);

cbuffer TextureIndices: register(b0) {
	uint4 textureIndices;
}

float4 PSMain(PSInput input) : SV_Target
{
	float4 color = t_albedo[textureIndices.x].Sample(t_sampler, input.uv);
	//float4 color = input.color;
	return pow(color, 1.0f / 2.2f);
}