#include "BasicShaderHeader.hlsli"


float4 BasicPS(Output input) : SV_TARGET
{
	if (input.instNo == 1)
	{
		return float4(0, 0, 0, 1);
	}

	float3 posFromLightVP = input.tpos.xyz / input.tpos.w;
	float2 shadowUV = (posFromLightVP + float2(1, -1)) * float2(0.5f, -0.5f);
	float depthFromLight = lightDepthTex.SampleCmp(shadowSmp, shadowUV, posFromLightVP.z - 0.005f);

	float shadowWeight = lerp(0.5f,1.0f, depthFromLight);

	// 光の向かうベクトル(平行光線)
	float3 light = normalize(float3(1,-1,1));

	// ライトのカラー(1,1,1で真っ白)
	float3 lightColor = float3(1, 1, 1);

	// ディフューズ計算
	float diffuseB = dot(-light, input.normal.xyz);
	float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	// 光の反射ベクトル
	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	// スフィアマップ用uv
	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);

	// テクスチャカラー
	float4 texColor = tex.Sample(smp, input.uv);

	float4 ret = max(
		toonDif // 輝度(トゥーン)
		* diffuse // ディフューズ色
		* texColor // テクスチャカラー
		* sph.Sample(smp, sphereMapUV) // スフィアマップ(乗算)
		+ spa.Sample(smp, sphereMapUV) * texColor // スフィアマップ(加算)
		+ float4(specularB * specular.rgb, 1) // スペキュラ
		, float4(ambient * texColor.rgb, 1)); // アンビエント

	return float4(ret.rgb * shadowWeight, ret.a);

	//return float4(brightness, brightness, brightness, 1) // 輝度
	//	* diffuse  // ディフューズ色
	//	* color // テクスチャカラー
	//	* sph.Sample(smp, sphereMapUV) // スフィアマップ(乗算)
	//	+ spa.Sample(smp, sphereMapUV) // スフィアマップ(加算)
	//	+ float4(color * ambient, 1) // アンビエント
	//	;
}