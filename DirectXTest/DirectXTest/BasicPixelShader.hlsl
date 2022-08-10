#include "BasicShaderHeader.hlsli"


float4 BasicPS(Output input) : SV_TARGET
{
	float3 light = normalize(float3(1,-1,1));
	float brightness = dot(-light, input.normal);
	float2 normalUV = (input.normal.xy + float2(1, -1)) * float2(0.5, 0.5);
	return float4(brightness, brightness, brightness, 1) // 輝度
		* diffuse  // ディフューズ色
		* tex.Sample(smp, input.uv) // テクスチャカラー
		* sph.Sample(smp, normalUV) // スフィアマップ(乗算)
		+ spa.Sample(smp, normalUV) // スフィアマップ(加算)
		; 
}