struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

Texture2D _texture : register(t0);
Texture2D _texture2 : register(t1);
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
    result.uv = input.uv;

    //result.position.y += sin(offset2.x) / 2.0f;//+offset2_2.x;
    //result.position.x += cos(offset2_2.x) / 2.0f;

    return result;
}

float4 PSMain(PSInput input) : SV_Target
{
    float4 color = _texture.Sample(_sampler, input.uv) / 10 + _texture2.Sample(_sampler, input.uv)+float4(sin(col.x) / 2.0f, 0, 0, 0);
    return lerp(color, pow(color, 1.0f/2.2f), input.uv.x > 0.5f ? 1.0f : 0.0f);//linear work fllow
}