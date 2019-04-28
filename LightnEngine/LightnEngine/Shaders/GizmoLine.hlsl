struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

struct VSInput {
	float3 startPosition : START_POSITION0;
	float3 endPosition : END_POSITION0;
	float4 color : COLOR0;
	uint InstanceId : SV_InstanceID;
};

cbuffer CameraInfo : register(b0)
{
	float4x4 mtxView;
	float4x4 mtxProj;
}

PSInput VSMain(VSInput input, uint vertexId : SV_VertexID)
{
	PSInput result;

	float4 worldPos = float4(lerp(input.startPosition, input.endPosition, vertexId), 1);
	float4 viewPos = mul(worldPos, mtxView);
	result.position = mul(viewPos, mtxProj);
	result.color = input.color;

	result.position = worldPos;

	return result;
}

float4 PSMain(PSInput input) : SV_Target
{
	float4 color = input.color;
	return pow(color, 1.0f / 2.2f);
}