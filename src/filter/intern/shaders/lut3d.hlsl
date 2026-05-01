#include "blend.hlsli"

Texture2D tex : register(t0);
Texture3D<float3> lut : register(t1);
SamplerState smp : register(s0);
cbuffer params : register(b0) {
    int blend_mode;
    float opacity;
    float should_clamp;
    float seed;
};

float4
main(float4 pos : SV_Position) : SV_Target {
    float3 size;
    lut.GetDimensions(size.x, size.y, size.z);

    float4 base = tex.Load(int3(pos.xy, 0));
    base.rgb *= rcp(max(base.a, 1.0e-4));

    const float3 uvw = mad(base.rgb, size.x - 1.0, 0.5) * rcp(size.x);
    const float4 src = float4(lut.Sample(smp, uvw), base.a);

    return blend(src, base, blend_mode, opacity, should_clamp, float4(pos.xy, seed, 0.0));
}
