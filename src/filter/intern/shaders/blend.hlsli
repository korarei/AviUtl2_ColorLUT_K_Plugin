static const float eps = 1.0e-4;

/*
The following function is a modified version of pcg4d function
Original implementation by Mark Jarzynski & Marc Olano
https://github.com/markjarzynski/PCG3D/blob/master/LICENSE
*/

uint4
pcg4d(uint4 v) {
    v = v * 1664525u + 1013904223u;

    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;

    v = v ^ v >> 16u;

    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;

    return v;
}

inline float
hash(float4 i) {
    return dot(pcg4d(uint4(i)), 1u) / 4294967295.0;
}

float3
rgb2hsy(float3 c) {
    const float4 k = float4(0.0, -rcp(3.0), 2.0 * rcp(3.0), -1.0);
    const float4 p = lerp(float4(c.bg, k.wz), float4(c.gb, k.xy), step(c.b, c.g));
    const float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    const float s = q.x - min(q.w, q.y);
    const float h = abs(mad(q.w - q.y, rcp(max(6.0 * s, eps)), q.z));
    const float y = dot(c, float3(0.3, 0.59, 0.11));
    return float3(h, s, y);
}

float3
hsy2rgb(float3 c) {
    const float4 k = float4(1.0, 2.0 * rcp(3.0), rcp(3.0), 3.0);
    const float3 p = abs(frac(c.xxx + k.xyz) * 6.0 - k.www);
    const float3 q = saturate(p - k.xxx) * c.y;
    c = q + c.z - dot(q, float3(0.3, 0.59, 0.11));
    const float l = dot(c, float3(0.3, 0.59, 0.11));
    const float n = min(min(c.r, c.g), c.b);
    const float x = max(max(c.r, c.g), c.b);
    c = mad(c - l, min(1.0, l * rcp(max(l - n, eps))), l);
    return mad(c - l, min(1.0, (1.0 - l) * rcp(max(x - l, eps))), l);
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


inline float3
soft_light(float3 base, float3 src) {
    const float3 d = lerp(sqrt(max(base, 0.0)), mad(mad(16.0, base, -12.0), base, 4.0) * base, step(base, 0.25));
    const float3 p = mad(2.0, src, -1.0);
    return lerp(mad(p, (d - base), base), mad(p, mad(base, -base, base), base), step(src, 0.5));
}

/*
inline float3
soft_light(float3 base, float3 src) {
    return mad(mad(base, -base, base), mad(2.0, src, -1.0), base);
}
*/

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
blend(float4 src, float4 base, int blend_mode, float opacity, float should_clamp, float4 seed) {
    src = saturate(src);
    base = saturate(base);

    if (blend_mode == 0) {
        src.rgb *= src.a;
        base.rgb *= base.a;
        src *= opacity;
        return mad(1.0 - src.a, base, src);
    } else if (blend_mode == 1) {
        base.rgb *= base.a;
        return lerp(base, float4(src.rgb, 1.0), step(hash(seed) + eps, src.a * opacity));
    }

    src.a *= opacity;

    float3 blended;
    [forcecase]
    switch (blend_mode) {
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

    const float3 rgb = max(mad(1.0 - src.a, base.rgb, mad(1.0 - base.a, src.rgb, blended)), 0.0);
    const float a = saturate(mad(1.0 - base.a, src.a, base.a));
    const float4 output = float4(rgb, a);

    return lerp(output, saturate(output), should_clamp);
}
