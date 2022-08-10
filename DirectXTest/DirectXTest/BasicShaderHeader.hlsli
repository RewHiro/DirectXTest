
// 頂点シェーダーからピクセルシェーダーへのやり取りに使用する構造体
struct Output
{
	float4 svpos : SV_POSITION; // システム用頂点座標
	float4 normal : NORMAL; // 法線ベクトル
	float2 uv : TEXCOORD; // uv値
};

Texture2D<float4> tex : register(t0); // 0番スロットに設定されたテクスチャ
SamplerState smp : register(s0); // 0番スロットに設定されたサンプラー

Texture2D<float4> sph : register(t1); // 1番スロットに設定されたテクスチャ
Texture2D<float4> spa : register(t2); // 2番スロットに設定されたテクスチャ

cbuffer cbuff0 : register(b0) // 定数バッファー
{
	matrix world; // ワールド変換行列
	matrix viewproj; // ビュープロジェクション行列
};

// 定数バッファー
// マテリアル用
cbuffer Material : register(b1)
{
	float4 diffuse; // ディフューズ色
	float4 specular; // スペキュラ
	float3 ambient; // アンビエント
};
