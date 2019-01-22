struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D _texture : register(t0);
SamplerState _sampler : register(s0);

cbuffer Constant2 : register(b0)
{
    float4 offset2;
    float dummy2[60];
}

PSInput VSMain(float4 position : POSITION, float4 uv : TEXCOORD)
{
    PSInput result;
    result.position = position;
    result.uv = uv;

    result.position.y += sin(offset2.x)/2.0f;

    return result;
}

float4 PSMain(PSInput input) : SV_Target
{
    float4 color = _texture.Sample(_sampler, input.uv);
    return lerp(color, pow(color, 1.0f/2.2f), input.uv.x > 0.5f ? 1.0f : 0.0f);//linear work fllow
}