Texture2D<float4> tex : register(t0); // �ʏ�e�N�X�`��
Texture2D<float4> texNormal : register(t1); // �@��
Texture2D<float4> texLum : register(t2); // ���P�x
Texture2D<float4> texShrinkHighLum : register(t3); // �k���o�b�t�@�[���P�x
Texture2D<float4> effectTex : register(t4);
Texture2D<float> depthTex : register(t5); // �f�v�X


SamplerState smp : register(s0); // �T���v���[

cbuffer PostEffect : register(b0)
{
	float4 bkweights[2];
}

struct Output
{
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};
