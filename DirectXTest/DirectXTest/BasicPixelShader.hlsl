float4 BasicPS(float4 pos : SV_POSITION ) : SV_TARGET
{
	return float4((float2(0, 1) + pos.xy) / float2(1920, 1080), 1.0f, 1.0f);
}