#define PI 3.1415926535897
#define EPSILON 1e-6

struct CameraInfo {
	float4x4 mtxView;
	float4x4 mtxProj;
	float3 cameraPos;
};

struct DirectionalLight {
	float intensity;
	float3 direction;
	float4 color;
};

struct PointLight {
	float4 position;
	float4 color;
	float4 attenuation;
};

//0~1のノーマルマップを-1~1の範囲にする
float3 DecodeNormalMapRG(in float2 normal) {
	float2 flipGNormal = normal;
	//flipGNormal.g = 1.0 - normal.g;

	// Expand the range of the normal value from (0, +1) to (-1, +1).
	float3 normalMap = float3(flipGNormal, 1.0);
	normalMap = (normalMap * 2.0f) - 1.0f;
	return normalMap;
}

//拡散反射BRDF
float3 DiffuseBRDF(in float3 diffuseColor)
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

	return lightScatter * viewScatter* energyFactor;
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
	return D * F* Vis / PI;
}

float3 FresnelSchlickRoughness(in float cosTheta, in float3 F0, in float roughness)
{
	float invR = 1 - roughness;
	return F0 + (max(float3(invR, invR, invR), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 SimpleToneMapping(in float3 color) {
	return color / (color + float3(1.0, 1.0, 1.0)); //ToneMapping
}

float3 ToLiner(in float3 value) {
	return pow(value, 2.2);
}

float3 ToGamma(in float3 value) {
	return pow(value, 1.0 / 2.2);
}