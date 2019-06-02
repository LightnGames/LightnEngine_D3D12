struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
    float3 viewDir : VIEWDIR;
};

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
};

cbuffer CameraInfo : register(b0)
{
    float4x4 mtxView;
    float4x4 mtxProj;
	float3 cameraPos;
}

cbuffer WorldMatrix : register(b1)
{
	float4x4 mtxWorld;
}

PSInput VSMain(VSInput input)
{
    PSInput result;

    float4 worldPos = mul(float4(input.position, 1), mtxWorld);
    float4 viewPos = mul(worldPos, mtxView);
    result.position = mul(viewPos, mtxProj);
    result.normal = mul(input.normal, (float3x3) input.mtxWorld);
    result.tangent = mul(input.tangent, (float3x3) input.mtxWorld);
    result.binormal = cross(result.normal, result.tangent);

    result.uv = input.uv;
    result.viewDir = normalize(cameraPos - worldPos.xyz);

    return result;
}