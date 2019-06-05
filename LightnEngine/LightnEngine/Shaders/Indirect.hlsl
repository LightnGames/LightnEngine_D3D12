#include "ShaderUtil.hlsl"

struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD;
	float3 viewDir : VIEWDIR;
	uint4 textureIndices :TEXTUREINDICES;
};

struct VSInput {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
	float4x4 mtxWorld : MATRIX0;
};

cbuffer CameraInfo : register(b0)
{
	CameraInfo camera;
}

cbuffer TextureIndices: register(b1) {
	uint4 textureIndices;
}

PSInput VSMain(VSInput input, uint vertexId : SV_VertexID)
{
	PSInput result;

	float4x4 mtxWorld = input.mtxWorld;
	float4 worldPos = mul(float4(input.position, 1), mtxWorld);

	float4 viewPos = mul(worldPos, camera.mtxView);
	result.position = mul(viewPos, camera.mtxProj);
	result.normal = normalize(mul(input.normal, (float3x3) mtxWorld));
	result.tangent = normalize(mul(input.tangent, (float3x3) mtxWorld));
	result.binormal = cross(result.normal, result.tangent);

	result.uv = input.uv;
	result.viewDir = normalize(camera.cameraPos - worldPos.xyz);
	result.textureIndices = textureIndices;

	return result;
}

TextureCube irradianceMap : register(t0);
TextureCube prefilterMap : register(t1);
Texture2D brdfLUT : register(t2);
Texture2D textures[] : register(t3);
SamplerState t_sampler : register(s0);

cbuffer DirectionalLightBuffer : register(b0){
	DirectionalLight directionalLight;
};

cbuffer PointLightBuffer : register(b1) {
	PointLight pointLight;
};

float4 PSMain(PSInput input) : SV_Target{
	float3 albedo = textures[input.textureIndices.x].Sample(t_sampler, input.uv).rgb;
	//float2 normal = t_normal.Sample(t_sampler, input.uv).rg;
	//float metallic = t_metallic.Sample(t_sampler, input.uv).r;
	//float roughness = t_roughness.Sample(t_sampler, input.uv).r;
	//float ao = t_ao.Sample(t_sampler, input.uv).r;

	float2 normal = float2(0.5,0.5);
	float metallic = 0;
	float roughness = 1;
	float ao = 1;

	//テクスチャを前準備
	albedo = ToLiner(albedo);
	float3 bumpMap = DecodeNormalMapRG(normal);

	//ノーマルマップを反映
	float3 N = (bumpMap.x * input.tangent) + (bumpMap.y * input.binormal) + (bumpMap.z * input.normal);
	N = normalize(N);
	//N = input.normal;
	//return float4(N, 1);

	//PBRベースカラーを算出
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

	//Direct Diffuse & Specular
	float3 directDiffuse = irradistance * DisneyDiffuseBRDF(dotNV, dotNL, dotLH, roughness) * DiffuseBRDF(diffuseColor);
	float3 directSpecular = irradistance * FrostbiteSupecularBRDF(dotNH, dotNL, dotNV, dotLH, specularColor, roughness);

	int maxMipLevels, width, height;
	prefilterMap.GetDimensions(0, width, height, maxMipLevels);

	float3 F = FresnelSchlickRoughness(dotNV, F0, roughness);
	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - metallic;

	//IBL Diffuse
	float3 irradiance = irradianceMap.SampleLevel(t_sampler, N, maxMipLevels).rgb;
	float3 envDiffuse = irradiance * albedo;

	//IBL Specular
	float3 prefilteredEnvColor = prefilterMap.SampleLevel(t_sampler, R, roughness * maxMipLevels).rgb;
	float2 envBRDF = brdfLUT.Sample(t_sampler, float2(dotNV, roughness)).rg;
	float3 envSpecular = prefilteredEnvColor * (F * envBRDF.x + envBRDF.y);

	float3 ambient = (kD * envDiffuse + envSpecular) * ao;
	ambient = ambient + directDiffuse + directSpecular;

	float3 color = lerp(ambient, ambient, 0.001);
	color = SimpleToneMapping(color);
	color = ToGamma(color);
	return float4(color.rgb, 1.0);
}