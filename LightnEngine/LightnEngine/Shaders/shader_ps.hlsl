#include "Shaders/ShaderUtil.h"

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
    float3 viewDir : VIEWDIR;
};

TextureCube irradianceMap : register(t0);
TextureCube prefilterMap : register(t1);
Texture2D brdfLUT : register(t2);
Texture2D t_albedo : register(t3);
Texture2D t_normal : register(t4);
Texture2D t_metallic : register(t5);
Texture2D t_roughness : register(t6);
Texture2D t_ao : register(t7);
SamplerState t_sampler : register(s0);

cbuffer DirectionalLightBuffer : register(b0)
{
	DirectionalLight directionalLight;
};

float4 PSMain(PSInput input) : SV_Target
{
    float3 albedo = t_albedo.Sample(t_sampler, input.uv).rgb;
    float2 normal = t_normal.Sample(t_sampler, input.uv).rg;
    float metallic = t_metallic.Sample(t_sampler, input.uv).r;
    float roughness = t_roughness.Sample(t_sampler, input.uv).r;
	float ao = t_ao.Sample(t_sampler, input.uv).r;

    albedo = pow(albedo, 2.2);

	//roughness = pow(roughness, 2.2);
	normal.g = 1.0 - normal.g;

    // Expand the range of the normal value from (0, +1) to (-1, +1).
    float3 bumpMap = float3(normal, 1.0);
    bumpMap = (bumpMap * 2.0f) - 1.0f;

    // Calculate the normal from the data in the bump map.
    float3 N = (bumpMap.x * input.tangent) + (bumpMap.y * input.binormal) + (bumpMap.z * input.normal);
    N = normalize(N);
    //N = input.normal;

    float3 diffuseColor = lerp(albedo.rgb, float3(0.04, 0.04, 0.04), metallic);
    float3 specularColor = lerp(float3(0.04, 0.04, 0.04), albedo.rgb, metallic);

	 //ライトベクトルと色を定義
    float3 L = -directionalLight.direction;
    float3 V = input.viewDir;
    float3 H = normalize(L + V);
    float3 R = reflect(-V, N);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float dotNL = saturate(dot(N, L));
    float dotNV = saturate(dot(N, V));
    float dotNH = saturate(dot(N, H));
    float dotVH = saturate(dot(V, H));
    float dotLH = saturate(dot(L, H));
    float3 irradistance = dotNL * directionalLight.color.xyz * directionalLight.intensity;
	//return float4(irradistance,1);

    //ライティング済みカラー＆スペキュラ
    float3 directDiffuse = irradistance * DisneyDiffuseBRDF(dotNV, dotNL, dotLH, roughness) * DiffuseBRDF(diffuseColor);
    float3 directSpecular = irradistance * FrostbiteSupecularBRDF(dotNH, dotNL, dotNV, dotLH, specularColor, roughness);

    int maxMipLevels, width, height;
    prefilterMap.GetDimensions(0, width, height, maxMipLevels);

    float3 F = FresnelSchlickRoughness(max(dotNV, 0.0), F0, roughness);
    float3 kS = F;
    float3 kD = float3(1.0,1.0,1.0) - kS;
    kD *= 1.0 - metallic;

    //Diffuse
    float3 irradiance = irradianceMap.SampleLevel(t_sampler, N, maxMipLevels).rgb;
    float3 envDiffuse = irradiance * albedo;

    //Specular
    float3 prefilteredEnvColor = prefilterMap.SampleLevel(t_sampler, R, roughness * maxMipLevels).rgb;
    float2 envBRDF = brdfLUT.Sample(t_sampler, float2(max(dotNV, 0.0), roughness)).rg;
    float3 envSpecular = prefilteredEnvColor * (F * envBRDF.x + envBRDF.y);

    float3 ambient = (kD * envDiffuse + envSpecular) * ao;
    ambient = ambient + directDiffuse + directSpecular;

    float3 color = lerp(ambient, ambient, 0.001);
    color = color / (color + float3(1.0,1.0,1.0)); //ToneMapping
    color = pow(color, 1.0 / 2.2); //linear work fllow;
    return float4(color.rgb, 1.0);

}