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

TextureCube irradianceMap : register(t0);
TextureCube prefilterMap : register(t1);
Texture2D brdfLUT : register(t2);
Texture2D t_albedo : register(t3);
Texture2D t_normal : register(t4);
Texture2D t_metallic : register(t5);
Texture2D t_roughness : register(t6);
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
    result.normal = mul(input.normal, (float3x3) mtxWorld);
    result.tangent = mul(input.tangent, (float3x3) mtxWorld);
    result.binormal = cross(result.normal, result.tangent);

    //result.position = input.position;
    result.uv = input.uv;
    result.viewDir = normalize(float3(0, 0, 0) - worldPos.xyz);

    //result.position.y += sin(offset2.x) / 2.0f;//+offset2_2.x;
    //result.position.x += cos(offset2_2.x) / 2.0f;

    return result;
}


float3 FresnelSchlickRoughness(in float cosTheta, in float3 F0, in float roughness)
{
    float invR = 1 - roughness;
    return F0 + (max(float3(invR, invR, invR), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float4 PSMain(PSInput input) : SV_Target
{
    float3 albedo = t_albedo.Sample(_sampler, input.uv).rgb;
    float2 normal = t_normal.Sample(_sampler, input.uv).rg;
    float metallic = t_metallic.Sample(_sampler, input.uv).r;
    float roughness = t_roughness.Sample(_sampler, input.uv).r;

    albedo = pow(albedo, 2.2);

    // Expand the range of the normal value from (0, +1) to (-1, +1).
    float3 bumpMap = float3(normal, 1.0);
    bumpMap = (bumpMap * 2.0f) - 1.0f;

    // Calculate the normal from the data in the bump map.
    float3 N = (bumpMap.x * input.tangent) + (bumpMap.y * input.binormal) + (bumpMap.z * input.normal);
    N = normalize(N);
    //N = input.normal;

    float3 V = input.viewDir;
    float3 R = reflect(-V, N);
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    int maxMipLevels, width, height;
    prefilterMap.GetDimensions(0, width, height, maxMipLevels);

    float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    float3 kS = F;
    float3 kD = float3(1.0,1.0,1.0) - kS;
    kD *= 1.0 - metallic;

    //Diffuse
    float3 irradiance = irradianceMap.SampleLevel(_sampler, N, maxMipLevels-1).rgb;
    float3 diffuse = irradiance * albedo;

    //Specular
    float3 prefilteredColor = prefilterMap.SampleLevel(_sampler, R, roughness * maxMipLevels).rgb;
    float2 envBRDF = brdfLUT.Sample(_sampler, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    float3 ambient = (kD * diffuse + specular); // * ao;

    float3 color = lerp(ambient, ambient, 0.001);
    color = color / (color + float3(1.0,1.0,1.0)); //ToneMapping
    color = pow(color, 1.0 / 2.2); //linear work fllow;
    return float4(color.rgb, 1.0);

}