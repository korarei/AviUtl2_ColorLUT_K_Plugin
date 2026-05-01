RWTexture2D<half4> dst : register(u0);
Texture2D src : register(t0);

[numthreads(32, 32, 1)]
void
main(uint3 pos : SV_DispatchThreadID) {
    uint2 size;
    src.GetDimensions(size.x, size.y);
    if (any(pos.xy >= size))
        return;

    dst[pos.xy] = src[pos.xy];
}
