#include "BasicShaderHeader.hlsli"


float4 BasicPS(Output input) : SV_TARGET
{
	// ���̌������x�N�g��(���s����)
	float3 light = normalize(float3(1,-1,1));

	// ���C�g�̃J���[(1,1,1�Ő^����)
	float3 lightColor = float3(1, 1, 1);

	// �f�B�t���[�Y�v�Z
	float diffuseB = dot(-light, input.normal.xyz);

	// ���̔��˃x�N�g��
	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	// �X�t�B�A�}�b�v�puv
	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);

	// �e�N�X�`���J���[
	float4 texColor = tex.Sample(smp, input.uv);

	return max(
		diffuseB // �P�x
		* diffuse // �f�B�t���[�Y�F
		* texColor // �e�N�X�`���J���[
		* sph.Sample(smp, sphereMapUV) // �X�t�B�A�}�b�v(��Z)
		+ spa.Sample(smp, sphereMapUV) * texColor // �X�t�B�A�}�b�v(���Z)
		+ float4(specularB * specular.rgb, 1) // �X�y�L����
		, float4(ambient * texColor, 1)); // �A���r�G���g

	//return float4(brightness, brightness, brightness, 1) // �P�x
	//	* diffuse  // �f�B�t���[�Y�F
	//	* color // �e�N�X�`���J���[
	//	* sph.Sample(smp, sphereMapUV) // �X�t�B�A�}�b�v(��Z)
	//	+ spa.Sample(smp, sphereMapUV) // �X�t�B�A�}�b�v(���Z)
	//	+ float4(color * ambient, 1) // �A���r�G���g
	//	;
}