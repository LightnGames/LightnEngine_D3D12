struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

struct VSInput {
	float4 position : POSITION;
	float4x4 mtxWorld : MATRIX0;
	float4 color : COLOR0;
	float height : HEIGHT0;
	uint InstanceId : SV_InstanceID;
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

	float heightOffset = lerp(-input.height, input.height, input.position.w);
	float4 localPosition = float4(input.position.xyz + float3(0, heightOffset, 0), 1);
	float4 worldPos = mul(localPosition, input.mtxWorld);
	float4 viewPos = mul(worldPos, mtxView);
	result.position = mul(viewPos, mtxProj);
	result.color = input.color;

	return result;
}

float4 PSMain(PSInput input) : SV_Target
{
	float4 color = input.color;
	return pow(color, 1.0f / 2.2f);
}