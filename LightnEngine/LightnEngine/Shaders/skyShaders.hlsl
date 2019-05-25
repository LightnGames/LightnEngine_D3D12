struct PSInput
{
    float4 position : SV_POSITION;
    float3 uv : TEXCOORD;
};

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
	float4x4 mtxWorld : MATRIX0;
};

TextureCube _texture : register(t0);
SamplerState _sampler : register(s0);

cbuffer CameraInfo : register(b0)
{
	float4x4 mtxView;
	float4x4 mtxProj;
	float3 cameraPos;
}

cbuffer ConstantPS : register(b2)
{
    float4 col;
    float dummy[60];
}

PSInput VSMain(VSInput input)
{
    PSInput result;

    float4 worldPos = mul(float4(input.position, 1), input.mtxWorld);
	float4 viewPos = float4(mul(worldPos.xyz, (float3x3)mtxView), 1);
    result.position = mul(viewPos, mtxProj);

    //result.position = input.position;
    result.uv = input.position;

    //result.position.y += sin(offset2.x) / 2.0f;//+offset2_2.x;
    //result.position.x += cos(offset2_2.x) / 2.0f;

    return result;
}

float4 PSMain(PSInput input) : SV_Target
{
    float4 color = _texture.Sample(_sampler, input.uv);
	color.rgb = color.rgb / (color.rgb + float3(1.0, 1.0, 1.0)); //ToneMapping
    return pow(color, 1.0f / 2.2f);
}