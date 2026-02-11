RWTexture2D<half4> dst : register(u0);
cbuffer params : register(b0) {
    uint level;
}

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint w, h;
    dst.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h)
        return;

    const uint size  = level * level;
    const uint idx = dtid.x + dtid.y * w;

    const uint r = idx % size;
    const uint g = (idx / size) % size;
    const uint b = idx / (size * size);

    dst[dtid.xy] = float4(float3(r, g, b) / float(size - 1u), 1.0);
}
