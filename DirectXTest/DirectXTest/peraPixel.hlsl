#include "peraHeader.hlsli"

float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy, float4 rect)
{
	float4 ret = tex.Sample(smp, uv);
	float l1 = -dx, l2 = -2 * dx;
	float r1 = dx, r2 = 2 * dx;
	float u1 = -dy, u2 = -2 * dy;
	float d1 = dy, d2 = 2 * dy;

	l1 = max(uv.x + l1, rect.x) - uv.x;
	l2 = max(uv.x + l2, rect.x) - uv.x;
	r1 = min(uv.x + r1, rect.z-dx) - uv.x;
	r2 = min(uv.x + r2, rect.z - dx) - uv.x;

	u1 = max(uv.y + u1, rect.y) - uv.y;
	u2 = max(uv.y + u2, rect.y) - uv.y;
	d1 = min(uv.y + d1, rect.w - dy) - uv.y;
	d2 = min(uv.y + d2, rect.w - dy) - uv.y;

	return float4((
			tex.Sample(smp, uv + float2(l2, u2)).rgb
		+ tex.Sample(smp, uv + float2(l1, u2)).rgb * 4
		+ tex.Sample(smp, uv + float2(0, u2)).rgb * 6
		+ tex.Sample(smp, uv + float2(r1, u2)).rgb * 4
		+ tex.Sample(smp, uv + float2(r2, u2)).rgb

		+ tex.Sample(smp, uv + float2(l2, u1)).rgb * 4
		+ tex.Sample(smp, uv + float2(l1, u1)).rgb * 16
		+ tex.Sample(smp, uv + float2(0, u1)).rgb * 24
		+ tex.Sample(smp, uv + float2(r1, u1)).rgb * 16
		+ tex.Sample(smp, uv + float2(r2, u1)).rgb * 4

		+ tex.Sample(smp, uv + float2(l2, 0)).rgb * 6
		+ tex.Sample(smp, uv + float2(l1, 0)).rgb * 24
		+ tex.Sample(smp, uv + float2(0, 0)).rgb * 36
		+ tex.Sample(smp, uv + float2(r1, 0)).rgb * 24
		+ tex.Sample(smp, uv + float2(r2, 0)).rgb * 6

		+ tex.Sample(smp, uv + float2(l2, d1)).rgb * 4
		+ tex.Sample(smp, uv + float2(l1, d1)).rgb * 16
		+ tex.Sample(smp, uv + float2(0, d1)).rgb * 24
		+ tex.Sample(smp, uv + float2(r1, d1)).rgb * 16
		+ tex.Sample(smp, uv + float2(r2, d1)).rgb * 4

		+ tex.Sample(smp, uv + float2(l2, d2)).rgb * 1
		+ tex.Sample(smp, uv + float2(l1, d2)).rgb * 4
		+ tex.Sample(smp, uv + float2(0, d2)).rgb * 6
		+ tex.Sample(smp, uv + float2(r1, d2)).rgb * 4
		+ tex.Sample(smp, uv + float2(r2, d2)).rgb * 1

		) / 256.0f, ret.a);
}

float4 main(Output input) : SV_TARGET
{
	//float dep = pow(depthTex.Sample(smp,input.uv), 20); // 20�悷��
	//return float4(dep, dep, dep, 1);

	if (input.uv.x < 0.2 && input.uv.y < 0.2)
	{
		float depth = depthTex.Sample(smp, input.uv * 5);
		depth = 1.0f - pow(depth, 30);
		return float4(depth, depth, depth, 1);
	}
	else if (input.uv.x < 0.2 && input.uv.y < 0.4)
	{
		return texShrinkHighLum.Sample(smp, (input.uv - float2(0, 0.2)) * 5);
	}
	else if (input.uv.x < 0.2 && input.uv.y < 0.6)
	{
		return dof.Sample(smp, (input.uv - float2(0, 0.4)) * 5);
	}
	else if (input.uv.x < 0.2 && input.uv.y < 0.8)
	{
		float s = texSSAO.Sample(smp, (input.uv - float2(0, 0.6)) * 5);
		return float4(s, s, s, 1);
	}

	float w, h, miplevels;
	tex.GetDimensions(0, w, h, miplevels);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float2 uvSize = float2(1, 0.5);
	float2 uvOfst = float2(0, 0);

	float4 bloomAccum = float4(0, 0, 0, 0);

	for (int i = 0; i < 8; i++)
	{
		bloomAccum += Get5x5GaussianBlur(texShrinkHighLum, smp, input.uv * uvSize + uvOfst, dx, dy, float4(uvOfst, uvOfst + uvSize));
		uvOfst.y += uvSize.y;
		uvSize *= 0.5f;
	}

	uvSize = float2(1, 0.5);
	uvOfst = float2(0, 0);

	// ��ʂ̐^�񒆂���̐[�x�̍��𑪂�
	float depthDiff = abs(depthTex.Sample(smp, float2(0.5f, 0.5f)) - depthTex.Sample(smp, input.uv));
	depthDiff = pow(depthDiff, 0.4f);
	float t = depthDiff * 8;
	float no;
	t = modf(t, no);

	float4 retColor[2];

	retColor[0] = tex.Sample(smp, input.uv); // �ʏ�e�N�X�`��

	float4 ao = texSSAO.Sample(smp, input.uv);

	retColor[0].xyz *= ao.xyz;

	if (no == 0.0f)
	{
		retColor[1] = Get5x5GaussianBlur(dof, smp, input.uv * uvSize + uvOfst, dx, dy, float4(uvOfst, uvOfst + uvSize));
	}
	else
	{
		for (int i = 1; i <= 8; ++i)
		{
			if (i - no < 0)
			{
				continue;
			}

			retColor[i - no] = Get5x5GaussianBlur(dof, smp, input.uv * uvSize + uvOfst, dx, dy, float4(uvOfst, uvOfst + uvSize));
			uvOfst.y += uvSize.y;
			uvSize *= 0.5f;

			if (i - no >= 1)
			{
				break;
			}
		}
	}

	//return retColor[0];

	return lerp(retColor[0], retColor[1], t);

	//return tex.Sample(smp, input.uv);

	return tex.Sample(smp, input.uv) + Get5x5GaussianBlur(texLum, smp, input.uv, dx, dy, float4(uvOfst, uvOfst + uvSize)) + saturate(bloomAccum);

	//float2 nmTex = effectTex.Sample(smp,input.uv).xy;
	//nmTex = nmTex * 2.0f - 1.0f;
	//return tex.Sample(smp, input.uv + nmTex * 0.1f);

	//float dx = 1.0f / 1280.0f;
	float4 ret = float4(0, 0, 0, 0);
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


	// �֊s�풊�o
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 0; // ����
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // ��
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // �E��
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)) * -1; // ��
	//ret += tex.Sample(smp, input.uv) * 4; // ����
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // �E
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // ����
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // ��
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 0; // �E��

	//float Y = dot(ret.rgb, float3(0.299, 0.587, 0.114));
	//Y = pow(1.0f - Y, 10.0f);
	//Y = step(0.2, Y);

	//return float4(Y, Y, Y, 1.0f);

	// �V���[�v�l�X
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 0; // ����
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // ��
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // �E��
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)) * -1; // ��
	//ret += tex.Sample(smp, input.uv) * 5; // ����
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // �E
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // ����
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // ��
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * 0; // �E��
	//return ret;

	// �G���{�X���H
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) * 2; // ����
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // ��
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)) * 0; // �E��
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)); // ��
	//ret += tex.Sample(smp, input.uv); // ����
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)) * -1; // �E
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) * 0; // ����
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)) * -1; // ��
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)) * -2; // �E��
	//return ret;

	// ��f���ω��ɂ��ڂ���
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)); // ����
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // ��
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy)); // �E��
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0)); // ��
	//ret += tex.Sample(smp, input.uv); // ����
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 0)); // �E
	//ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)); // ����
	//ret += tex.Sample(smp, input.uv + float2(0, -2 * dy)); // ��
	//ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy)); // �E��
	//return ret / 9.0f;


	// ���m�N��
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

BlurOutput BlurPS(Output input)
{
	float w, h, miplevels;
	tex.GetDimensions(0, w, h, miplevels);

	float2 uvSize = float2(1, 0.5);
	float2 uvOfst = float2(0, 0);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	BlurOutput output;

	output.col = Get5x5GaussianBlur(tex, smp, input.uv, dx, dy, float4(0, 0, 1, 1));
	output.highLum = Get5x5GaussianBlur(texLum, smp, input.uv, dx, dy, float4(0, 0, 1, 1));

	return output;
}
