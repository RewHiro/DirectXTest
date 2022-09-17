Texture2D<float4> tex : register(t0); // 通常テクスチャ
Texture2D<float4> texNormal : register(t1); // 法線
Texture2D<float4> texLum : register(t2); // 高輝度
Texture2D<float4> texShrinkHighLum : register(t3); // 縮小バッファー高輝度
Texture2D<float4> effectTex : register(t4);
Texture2D<float> depthTex : register(t5); // デプス


SamplerState smp : register(s0); // サンプラー

cbuffer PostEffect : register(b0)
{
	float4 bkweights[2];
}

struct Output
{
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};
