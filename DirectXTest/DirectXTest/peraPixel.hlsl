#include "peraHeader.hlsli"

float4 main(Output input) : SV_TARGET
{
	float2 nmTex = effectTex.Sample(smp,input.uv).xy;
	nmTex = nmTex * 2.0f - 1.0f;
	return tex.Sample(smp, input.uv + nmTex * 0.1f);

	//float dx = 1.0f / 1280.0f;
	//float4 ret = float4(0, 0, 0, 0);
	//float4 col = tex.Sample(smp, input.uv);

	//ret += bkweights[0] * col;

	//for (int i = 1; i < 8; ++i)
	//{
	//	ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(i * dx, 0));
	//	ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(-i * dx, 0));
	//}

	//return float4(ret.rgb, col.a);

	//float dx = 1.0f / 1280.0f;
	//float dy = 1.0f / 720.0f;

	//float dx = 2.0f / 1280.0f;
	//float dy = 2.0f / 720.0f;
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 1 / 256;
	//ret += tex.Sample(smp, input.uv + float2(-1 * dx, 2 * dy)) * 4 / 256;
	//ret += tex.Sample(smp, input.uv + float2(0 * dx, 2 * dy)) * 6 / 256;
	//ret += tex.Sample(smp, input.uv + float2(1 * dx, 2 * dy)) * 4 / 256;
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 1 / 256;

	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 1 * dy)) * 4 / 256;
	//ret += tex.Sample(smp, input.uv + float2(-1 * dx, 1 * dy)) * 16 / 256;
	//ret += tex.Sample(smp, input.uv + float2(0 * dx, 1 * dy)) * 24 / 256;
	//ret += tex.Sample(smp, input.uv + float2(1 * dx, 1 * dy)) * 16 / 256;
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 1 * dy)) * 4 / 256;

	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0 * dy)) * 6 / 256;
	//ret += tex.Sample(smp, input.uv + float2(-1 * dx, 0 * dy)) * 24 / 256;
	//ret += tex.Sample(smp, input.uv + float2(0 * dx, 0 * dy)) * 36 / 256;
	//ret += tex.Sample(smp, input.uv + float2(1 * dx, 0 * dy)) * 24 / 256;
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0 * dy)) * 6 / 256;

	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -1 * dy)) * 4 / 256;
	//ret += tex.Sample(smp, input.uv + float2(-1 * dx, -1 * dy)) * 16 / 256;
	//ret += tex.Sample(smp, input.uv + float2(0 * dx, -1 * dy)) * 24 / 256;
	//ret += tex.Sample(smp, input.uv + float2(1 * dx, -1 * dy)) * 16 / 256;
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -1 * dy)) * 4 / 256;

	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 1 / 256;
	//ret += tex.Sample(smp, input.uv + float2(-1 * dx, -2 * dy)) * 4 / 256;
	//ret += tex.Sample(smp, input.uv + float2(0 * dx, -2 * dy)) * 6 / 256;
	//ret += tex.Sample(smp, input.uv + float2(1 * dx, -2 * dy)) * 4 / 256;
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 1 / 256;

	//return float4(ret.rgb, 1.0f);


	// 輪郭戦抽出
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 0; // 左上
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // 上
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // 右上
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)) * -1; // 左
	//ret += tex.Sample(smp, input.uv) * 4; // 自分
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // 右
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // 左下
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // 下
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 0; // 右下

	//float Y = dot(ret.rgb, float3(0.299, 0.587, 0.114));
	//Y = pow(1.0f - Y, 10.0f);
	//Y = step(0.2, Y);

	//return float4(Y, Y, Y, 1.0f);

	// シャープネス
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 0; // 左上
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // 上
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // 右上
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)) * -1; // 左
	//ret += tex.Sample(smp, input.uv) * 5; // 自分
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // 右
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // 左下
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // 下
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 0; // 右下
	//return ret;

	// エンボス加工
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 2; // 左上
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // 上
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // 右上
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)); // 左
	//ret += tex.Sample(smp, input.uv); // 自分
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // 右
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // 左下
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // 下
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * -2; // 右下
	//return ret;

	// 画素平均化によるぼかし
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)); // 左上
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // 上
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)); // 右上
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)); // 左
	//ret += tex.Sample(smp, input.uv); // 自分
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)); // 右
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)); // 左下
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // 下
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)); // 右下
	//return ret / 9.0f;


	// モノクロ
	//float4 col = tex.Sample(smp, input.uv);
	//float Y = dot(col.rgb, float3(0.299, 0.587, 0.114));
	//return float4(col.rgb - fmod(col.rgb,0.25f), col.a);
}

float4 VerticalBokehPS(Output input) : SV_TARGET
{
	float dy = 1.0f / 720.0f;
	float4 ret = float4(0, 0, 0, 0);
	float4 col = tex.Sample(smp, input.uv);

	ret += bkweights[0] * col;

	for (int i = 1; i < 8; ++i)
	{
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0, i * dy));
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0, -i * dy));
	}

	return float4(ret.rgb, col.a);
}
