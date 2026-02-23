// D2DのBlendEffectとは違いHDR前提の合成
Texture2D fg : register(t0);
Texture2D bg : register(t1);
SamplerState fg_smp : register(s0);
SamplerState bg_smp : register(s1);

cbuffer params : register(b0) {
    int mode;
    float opacity;
    bool clamp_output;
};

static const float eps = 1.0e-4;

struct PS_Input {
    float4 dummy : SV_Position; // D2Dの仕様で使用不可
    float4 pos : SCENE_POSITION;
    float4 fg_uv : TEXCOORD0;
    float4 bg_uv : TEXCOORD1;
};

float3
rgb2hsy(float3 c) {
    const float4 k = float4(0.0, -1.0 * rcp(3.0), 2.0 * rcp(3.0), -1.0);
    float4 p = lerp(float4(c.bg, k.wz), float4(c.gb, k.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float s = q.x - min(q.w, q.y);
    float h = abs(mad(q.w - q.y, rcp(mad(6.0, s, eps)), q.z));
    float y = dot(c, float3(0.3, 0.59, 0.11));
    return float3(h, s, y);
}

float3
hsy2rgb(float3 c) {
    float3 rgb = saturate(float3(abs(mad(c.x, 6.0, -3.0)) - 1.0, 2.0 - abs(mad(c.x, 6.0, -2.0)), 2.0 - abs(mad(c.x, 6.0, -4.0)))) * c.y;
    return rgb + c.z - dot(rgb, float3(0.3, 0.59, 0.11));
}

inline float3
darken(float3 base, float3 src) {
    return min(base, src);
}

inline float3
multiply(float3 base, float3 src) {
    return base * src;
}

inline float3
color_burn(float3 base, float3 src) {
    return lerp(saturate(mad(base - 1.0, rcp(max(src, eps)), 1.0)), 0.0, step(src, 0.0));
}

inline float3
linear_burn(float3 base, float3 src) {
    return base + src - 1.0;
}

inline float3
darker_color(float3 base, float3 src) {
    return lerp(base, src, step(dot(src, 1.0), dot(base, 1.0)));
}

inline float3
lighten(float3 base, float3 src) {
    return max(base, src);
}

inline float3
screen(float3 base, float3 src) {
    return mad(base - 1.0, 1.0 - src, 1.0);
}

inline float3
color_dodge(float3 base, float3 src) {
    return lerp(lerp(min(base * rcp(max(1.0 - src, eps)), 1.0), 1.0, step(1.0, src)), 0.0, step(abs(base), eps));
}

inline float3
linear_dodge(float3 base, float3 src) {
    return base + src;
}

inline float3
lighter_color(float3 base, float3 src) {
    return lerp(src, base, step(dot(src, 1.0), dot(base, 1.0)));
}

inline float3
overlay(float3 base, float3 src) {
    return lerp(2.0 * base * src, mad(mad(2.0, base, -2.0), 1.0 - src, 1.0), step(0.5, base));
}

/*
inline float3
soft_light(float3 base, float3 src) {
    const float3 d = lerp(sqrt(max(base, 0.0)), ((16.0 * base - 12.0) * base + 4.0) * base, step(base, 0.25));
    return lerp(base + (2.0 * src - 1.0) * (d - base), base - (1.0 - 2.0 * src) * base * (1.0 - base), step(src, 0.5));
}
*/

inline float3
soft_light(float3 base, float3 src) {
    return mad(mad(base, -base, base), mad(2.0, src, -1.0), base);
}

inline float3
hard_light(float3 base, float3 src) {
    return overlay(src, base);
}

inline float3
linear_light(float3 base, float3 src) {
    return mad(2.0, src, base - 1.0);
}

inline float3
vivid_light(float3 base, float3 src) {
    return lerp(color_burn(base, 2.0 * src), color_dodge(base, mad(2.0, src, -1.0)), step(0.5, src));
}

inline float3
pin_light(float3 base, float3 src) {
    return lerp(min(base, 2.0 * src), max(base, mad(2.0, src, -1.0)), step(0.5, src));
}

inline float3
hard_mix(float3 base, float3 src) {
    return step(1.0, base + src);
}

inline float3
difference(float3 base, float3 src) {
    return abs(base - src);
}

inline float3
exclusion(float3 base, float3 src) {
    return mad(-2.0, base * src, base + src);
}

inline float3
subtract(float3 base, float3 src) {
    return base - src;
}

inline float3
divide(float3 base, float3 src) {
    return base * rcp(max(abs(src), eps) * mad(2.0, step(0.0, src), -1.0));
}

inline float3
hue(float3 base, float3 src) {
    return hsy2rgb(float3(rgb2hsy(src).x, rgb2hsy(base).yz));
}

inline float3
saturation(float3 base, float3 src) {
    return hsy2rgb(float3(rgb2hsy(base).x, rgb2hsy(src).y, rgb2hsy(base).z));
}

inline float3
color(float3 base, float3 src) {
    return hsy2rgb(float3(rgb2hsy(src).xy, rgb2hsy(base).z));
}

inline float3
luminosity(float3 base, float3 src) {
    return hsy2rgb(float3(rgb2hsy(base).xy, rgb2hsy(src).z));
}

float4
main(PS_Input input) : SV_Target {
    float4 src = fg.Sample(fg_smp, input.fg_uv.xy);
    float4 base  = bg.Sample(bg_smp, input.bg_uv.xy);

    if (mode == 0) {
        src *= opacity;
        const float4 output = mad(1.0 - src.a, base, src);
        return lerp(output, saturate(output), clamp_output);
    }

    src  = float4(src.rgb  * rcp(max(src.a,  eps)), src.a * opacity);
    base = float4(base.rgb * rcp(max(base.a, eps)), base.a);

    float3 blended;
    [forcecase]
    switch (mode) {
        case 2:
            blended = darken(base.rgb, src.rgb);
            break;
        case 3:
            blended = multiply(base.rgb, src.rgb);
            break;
        case 4:
            blended = color_burn(base.rgb, src.rgb);
            break;
        case 5:
            blended = linear_burn(base.rgb, src.rgb);
            break;
        case 6:
            blended = darker_color(base.rgb, src.rgb);
            break;
        case 7:
            blended = lighten(base.rgb, src.rgb);
            break;
        case 8:
            blended = screen(base.rgb, src.rgb);
            break;
        case 9:
            blended = color_dodge(base.rgb, src.rgb);
            break;
        case 10:
            blended = linear_dodge(base.rgb, src.rgb);
            break;
        case 11:
            blended = lighter_color(base.rgb, src.rgb);
            break;
        case 12:
            blended = overlay(base.rgb, src.rgb);
            break;
        case 13:
            blended = soft_light(base.rgb, src.rgb);
            break;
        case 14:
            blended = hard_light(base.rgb, src.rgb);
            break;
        case 15:
            blended = linear_light(base.rgb, src.rgb);
            break;
        case 16:
            blended = vivid_light(base.rgb, src.rgb);
            break;
        case 17:
            blended = pin_light(base.rgb, src.rgb);
            break;
        case 18:
            blended = hard_mix(base.rgb, src.rgb);
            break;
        case 19:
            blended = difference(base.rgb, src.rgb);
            break;
        case 20:
            blended = exclusion(base.rgb, src.rgb);
            break;
        case 21:
            blended = subtract(base.rgb, src.rgb);
            break;
        case 22:
            blended = divide(base.rgb, src.rgb);
            break;
        case 23:
            blended = hue(base.rgb, src.rgb);
            break;
        case 24:
            blended = saturation(base.rgb, src.rgb);
            break;
        case 25:
            blended = color(base.rgb, src.rgb);
            break;
        case 26:
            blended = luminosity(base.rgb, src.rgb);
            break;
        default:
            blended = src.rgb;
            break;
    }

    blended *= src.a * base.a;
    src.rgb *= src.a;
    base.rgb *= base.a;

    const float3 rgb = mad(1.0 - src.a, base.rgb, mad(1.0 - base.a, src.rgb, blended));
    const float a = mad(1.0 - base.a, src.a, base.a);
    const float4 output = float4(rgb, a);

    return lerp(output, saturate(output), clamp_output);
}
