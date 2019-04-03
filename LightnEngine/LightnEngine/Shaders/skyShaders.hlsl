struct PSInput
{
    float4 position : SV_POSITION;
    float3 uv : TEXCOORD;
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

TextureCube _texture : register(t0);
SamplerState _sampler : register(s0);

cbuffer Constant1 : register(b0)
{
    float4 offset2;
    float2 offset3;
    float4x4 mtxWorld;
    float4x4 mtxView;
    float4x4 mtxProj;
    float dummy2[10];
}

cbuffer Constant2 : register(b1)
{
    float2 offset2_2;
    float dummy2_2[62];
}

cbuffer ConstantPS : register(b2)
{
    float4 col;
    float dummy[60];
}

PSInput VSMain(VSInput input)
{
    PSInput result;

    float4 worldPos = mul(float4(input.position, 1), mtxWorld);
    float4 viewPos = mul(worldPos, mtxView);
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
    return pow(color, 1.0f / 2.2f);
}