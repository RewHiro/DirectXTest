#include "BasicShaderHeader.hlsli"


PixelOutput BasicPS(Output input)
{
	if (input.instNo == 1)
	{
		PixelOutput output;
		output.col = float4(0,0,0,1);
		output.normal = float4(0, 0, 0, 1);
		return output;
	}

	float3 posFromLightVP = input.tpos.xyz / input.tpos.w;
	float2 shadowUV = (posFromLightVP + float2(1, -1)) * float2(0.5f, -0.5f);
	float depthFromLight = lightDepthTex.SampleCmp(shadowSmp, shadowUV, posFromLightVP.z - 0.005f);

	float shadowWeight = lerp(0.5f,1.0f, depthFromLight);

	// ���̌������x�N�g��(���s����)
	float3 light = normalize(float3(1,-1,1));

	// ���C�g�̃J���[(1,1,1�Ő^����)
	float3 lightColor = float3(1, 1, 1);

	// �f�B�t���[�Y�v�Z
	float diffuseB = dot(-light, input.normal.xyz);
	float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	// ���̔��˃x�N�g��
	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	// �X�t�B�A�}�b�v�puv
	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);

	// �e�N�X�`���J���[
	float4 texColor = tex.Sample(smp, input.uv);

	float4 ret = max(
		toonDif // �P�x(�g�D�[��)
		* diffuse // �f�B�t���[�Y�F
		* texColor // �e�N�X�`���J���[
		* sph.Sample(smp, sphereMapUV) // �X�t�B�A�}�b�v(��Z)
		+ spa.Sample(smp, sphereMapUV) * texColor // �X�t�B�A�}�b�v(���Z)
		+ float4(specularB * specular.rgb, 1) // �X�y�L����
		, float4(ambient * texColor.rgb, 1)); // �A���r�G���g

	PixelOutput output;
	output.col = float4(ret.rgb * shadowWeight, ret.a);
	output.normal.rgb = float3((input.normal.xyz + 1.0f) / 2.0f);
	output.normal.a = 1;
	return output;

	//return float4(brightness, brightness, brightness, 1) // �P�x
	//	* diffuse  // �f�B�t���[�Y�F
	//	* color // �e�N�X�`���J���[
	//	* sph.Sample(smp, sphereMapUV) // �X�t�B�A�}�b�v(��Z)
	//	+ spa.Sample(smp, sphereMapUV) // �X�t�B�A�}�b�v(���Z)
	//	+ float4(color * ambient, 1) // �A���r�G���g
	//	;
}