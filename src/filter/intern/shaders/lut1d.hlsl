#include "blend.hlsli"

Texture2D tex : register(t0);
Texture1DArray<float> lut : register(t1);
SamplerState smp : register(s0);
cbuffer params : register(b0) {
    float size;
    int blend_mode;
    float opacity;
    float should_clamp;
    float seed;
}

float4
main(float4 pos : SV_Position) : SV_Target {
    float4 base = tex.Load(int3(pos.xy, 0));
    base.rgb *= rcp(max(base.a, 1.0e-4));

    const float3 uvw = mad(base.rgb, size - 1.0, 0.5) * rcp(size);
    const float r = lut.Sample(smp, float2(uvw.r, 0.0));
    const float g = lut.Sample(smp, float2(uvw.g, 1.0));
    const float b = lut.Sample(smp, float2(uvw.b, 2.0));

    return blend(float4(r, g, b, base.a), base, blend_mode, opacity, should_clamp, float4(pos.xy, seed, 0.0));
}
