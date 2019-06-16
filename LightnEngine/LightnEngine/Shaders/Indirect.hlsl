#include "ShaderUtil.hlsl"

struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD;
	float3 viewDir : VIEWDIR;
	float3 worldPosition :WORLDPOSITION;
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
	result.worldPosition = worldPos.xyz;

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

float getSquareFalloffAttenuation(float distanceSquare, float falloff) {
	float factor = distanceSquare * falloff;
	float smoothFactor = saturate(1.0 - factor * factor);
	// We would normally divide by the square distance here
	// but we do it at the call site
	return smoothFactor * smoothFactor;
}

float getDistanceAttenuation(float3 posToLight, float falloff, float distancePower) {
	float distanceSquare = dot(posToLight, posToLight) * distancePower;
	float attenuation = getSquareFalloffAttenuation(distanceSquare, falloff);
	// Assume a punctual light occupies a volume of 1cm to avoid a division by 0
	return attenuation * 1.0 / max(distanceSquare, EPSILON);
}

float computePreExposedIntensity(float intensity, float exposure) {
	return intensity * exposure;
}

float4 PSMain(PSInput input) : SV_Target{
	float3 albedo = textures[input.textureIndices.x].Sample(t_sampler, input.uv).rgb;
	float2 normalMap = textures[input.textureIndices.y].Sample(t_sampler, input.uv).rg;
	float3 arm = textures[input.textureIndices.z].Sample(t_sampler, input.uv).rgb;

	float metallic = arm.b;
	float roughness = arm.g;
	float ao = arm.r;
	//normalMap = float2(0.5,0.5);
	//metallic = 1;
	//roughness = 0;

	//return float4(normalMap,1, 1);
	//return float4(ao, 0, 0, 1);

	//テクスチャを前準備
	albedo = ToLiner(albedo);
	float3 bumpMap = DecodeNormalMapRG(normalMap);

	//ノーマルマップを反映
	float3 N = (bumpMap.x * input.tangent) + (bumpMap.y * input.binormal) + (bumpMap.z * input.normal);
	N = normalize(N);
	//return float4(N / 2 + 0.5, 1);

	//PBRベースカラーを算出
	float3 diffuseColor = lerp(albedo, float3(0.04, 0.04, 0.04), metallic);
	float3 specularColor = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

	//ライトベクトルと色を定義
	float3 L = -directionalLight.direction;
	float3 V = input.viewDir;
	float3 H = normalize(L + V);
	float3 R = reflect(-V, N);

	float dotNV = saturate(dot(N, V));
	float dotNL = saturate(dot(N, L));
	float dotNH = saturate(dot(N, H));
	float dotVH = saturate(dot(V, H));
	float dotLH = saturate(dot(L, H));
	float3 irradistance = dotNL * directionalLight.color.rgb * directionalLight.intensity;
	//return float4(irradistance,1);

	//Direct Diffuse & Specular
	float3 directDiffuse = DisneyDiffuseBRDF(dotNV, dotNL, dotLH, roughness) * diffuseColor;
	float3 directSpecular = FrostbiteSupecularBRDF(dotNH, dotNL, dotNV, dotLH, specularColor, roughness);
	float3 directionalResult = (directDiffuse + directSpecular) * irradistance;

	float3 posToLightP = pointLight.position.xyz - input.worldPosition;
	float3 Lp = normalize(posToLightP);
	float3 Hp = normalize(Lp + V);
	float dotNLp = saturate(dot(N, Lp));
	float dotNHp = saturate(dot(N, Hp));
	float dotVHp = saturate(dot(V, Hp));
	float dotLHp = saturate(dot(Lp, Hp));
	float pointLightAttr = getDistanceAttenuation(posToLightP, pointLight.attenuation.x, pointLight.attenuation.y);
	float3 pointIrradistance = pointLight.color.rgb * pointLight.color.a * dotNLp * pointLightAttr;// *computePreExposedIntensity(pointLight.attenuation.y, pointLight.attenuation.z);
	float3 pointDirectDiffuse = DisneyDiffuseBRDF(dotNV, dotNLp, dotLHp, roughness) * diffuseColor;
	float3 pointDirectSpecular = FrostbiteSupecularBRDF(dotNHp, dotNLp, dotNV, dotLHp, specularColor, roughness);
	float3 pointLightResult = (pointDirectDiffuse + pointDirectSpecular)* pointIrradistance;
	//float db = dotNLp * pointLightAttr;
	//return float4(db, db, db, 1);
	//return float4(pointIrradistance, 1);

	int maxMipLevels, width, height;
	prefilterMap.GetDimensions(0, width, height, maxMipLevels);

	float3 F = FresnelSchlickRoughness(dotNV, specularColor, roughness);
	//return float4(F, 1);
	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - metallic;

	//IBL Diffuse
	float3 irradiance = irradianceMap.SampleLevel(t_sampler, N, maxMipLevels).rgb;
	irradiance = ToLiner(irradiance);
	float3 envDiffuse = irradiance * diffuseColor;

	//IBL Specular
	float3 prefilteredEnvColor = prefilterMap.SampleLevel(t_sampler, R, roughness * maxMipLevels).rgb;
	prefilteredEnvColor = ToLiner(prefilteredEnvColor);
	float2 envBRDF = brdfLUT.Sample(t_sampler, float2(dotNV, roughness)).rg;
	float3 envSpecular = prefilteredEnvColor * (F * envBRDF.x + envBRDF.y)*0.2;//HDRだとスペキュラが明るすぎる？
	//return float4(envSpecular, 1);

	float3 ambient = (kD * envDiffuse + envSpecular) * ao;
	//return float4(ToGamma(ambient), 1);
	ambient += directionalResult + pointLightResult;

	float3 color = lerp(ambient, ambient, 0.0);
	color = SimpleToneMapping(color);
	color = ToGamma(color);
	return float4(color.rgb, 1.0);
}