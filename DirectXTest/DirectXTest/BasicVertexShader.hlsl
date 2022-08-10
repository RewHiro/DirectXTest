#include "BasicShaderHeader.hlsli"

Output BasicVS
(
	float4 pos : POSITION, 
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONE_NO,
	min16uint weight : WEIGHT
)
{
	Output output; // �s�N�Z���V�F�[�_�[�ɓn���l
	output.svpos = mul(mul(mul(proj, view), world), pos); // �V�F�[�_�[�ł͗�D��Ȃ̂Œ���
	output.uv = uv;
	normal.w = 0; // �������d�v(���s�ړ������𖳌��ɂ���)
	output.normal = mul(world, normal); // �@���ɂ����[���h�ϊ����s��
	output.vnormal = mul(view, output.normal);
	output.ray = normalize(pos.xyz - eye); // �����x�N�g��
	return output;
}