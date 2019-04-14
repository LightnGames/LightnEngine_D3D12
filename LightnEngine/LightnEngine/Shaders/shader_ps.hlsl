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
SamplerState t_sampler : register(s0);

#define PI 3.1415926535897
#define EPSILON 1e-6

cbuffer DirectionalLightBuffer : register(b0)
{
    float intensity;
    float3 direction;
};

cbuffer ROOT_32BIT_CONSTANTS_DirectionalLight : register(b1)
{
	float4 color;
};

//拡散反射BRDF
float3 DiffuseBRDF(float3 diffuseColor)
{
    return diffuseColor / PI;
}

//フレネル
float3 F_Schlick(in float3 f0, in float f90, in float u)
{
    return (f0 + (f90 - f0) * pow(1.0 - u, 5.0));
}

//Frostbyte3.0 DisnyDiffuse改良版 https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
float DisneyDiffuseBRDF(float dotNV, float dotNL, float dotLH, float linearRoughness)
{
    float energyBias = lerp(0, 0.5, linearRoughness);
    float energyFactor = lerp(1.0, 1.0 / 1.51, linearRoughness); //従来のDisnyDiffuseではエネルギー保存の法則を満たさないので、最大ピークの1.5を正規化する
    float fd90 = energyBias + 2.0 * dotLH * dotLH * linearRoughness;
    float3 f0 = float3(1.0f, 1.0f, 1.0f);

    float lightScatter = F_Schlick(f0, fd90, dotNL).r;
    float viewScatter = F_Schlick(f0, fd90, dotNV).r;

    return lightScatter * viewScatter * energyFactor;
}

float V_SmithGGXCorralated(float dotNL, float dotNV, float alphaG)
{
    float alphaG2 = alphaG * alphaG;
    float lambda_GGXV = dotNL * sqrt((-dotNV * alphaG2 + dotNV) * dotNV + alphaG2);
    float lambda_GGXL = dotNV * sqrt((-dotNL * alphaG2 + dotNL) * dotNL + alphaG2);

    return saturate(0.5f / lambda_GGXV + lambda_GGXL); //本来saturateはいらないはず。。。
}

//マイクロファセット分布関数
float D_GGX(in float a, in float dotNH)
{
    float a2 = a * a;

    //Frostbite3.0
    float d = (dotNH * a2 - dotNH) * dotNH + 1;
    return a2 / (d * d);
}

//Frostbite3.0 Specular
float FrostbiteSupecularBRDF(float dotNH, float dotNL, float dotNV, float dotLH, float3 specularColor, float alpha)
{
    float energyBias = lerp(0, 0.5, alpha);
    float energyFactor = lerp(1.0, 1.0 / 1.51, alpha);
    float fd90 = energyBias + 2.0 * dotLH * dotLH * alpha;
    float3 F = F_Schlick(specularColor, fd90, dotLH);
    float Vis = V_SmithGGXCorralated(dotNV, dotNL, alpha);
    float D = D_GGX(alpha, dotNH);
    return D * F * Vis / PI;
}

float3 FresnelSchlickRoughness(in float cosTheta, in float3 F0, in float roughness)
{
    float invR = 1 - roughness;
    return F0 + (max(float3(invR, invR, invR), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float4 PSMain(PSInput input) : SV_Target
{
    float3 albedo = t_albedo.Sample(t_sampler, input.uv).rgb;
    float2 normal = t_normal.Sample(t_sampler, input.uv).rg;
    float metallic = t_metallic.Sample(t_sampler, input.uv).r;
    float roughness = t_roughness.Sample(t_sampler, input.uv).r;

    albedo = pow(albedo, 2.2);

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
    float3 L = -direction;
    float3 V = input.viewDir;
    float3 H = normalize(L + V);
    float3 R = reflect(-V, N);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float dotNL = saturate(dot(N, L));
    float dotNV = saturate(dot(N, V));
    float dotNH = saturate(dot(N, H));
    float dotVH = saturate(dot(V, H));
    float dotLH = saturate(dot(L, H));
    float3 irradistance = dotNL * color.xyz * intensity;
	//return float4(irradistance,1);

    //ライティング済みカラー＆スペキュラ
    float3 directDiffuse = irradistance * DisneyDiffuseBRDF(dotNV, dotNL, dotLH, roughness) * DiffuseBRDF(diffuseColor);
    float3 directSpecular = irradistance * FrostbiteSupecularBRDF(dotNH, dotNL, dotNV, dotLH, specularColor, roughness);

    int maxMipLevels, width, height;
    prefilterMap.GetDimensions(0, width, height, maxMipLevels);

    float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    float3 kS = F;
    float3 kD = float3(1.0,1.0,1.0) - kS;
    kD *= 1.0 - metallic;

    //Diffuse
    float3 irradiance = irradianceMap.SampleLevel(t_sampler, N, maxMipLevels-1).rgb;
    float3 envDiffuse = irradiance * albedo;

    //Specular
    float3 prefilteredEnvColor = prefilterMap.SampleLevel(t_sampler, R, roughness * maxMipLevels).rgb;
    float2 envBRDF = brdfLUT.Sample(t_sampler, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 envSpecular = prefilteredEnvColor * (F * envBRDF.x + envBRDF.y);

    float3 ambient = (kD * envDiffuse + envSpecular); // * ao;
    ambient = ambient + directDiffuse + directSpecular;

    float3 color = lerp(ambient + directSpecular, ambient, 0.001);
    color = color / (color + float3(1.0,1.0,1.0)); //ToneMapping
    color = pow(color, 1.0 / 2.2); //linear work fllow;
    return float4(color.rgb, 1.0);

}