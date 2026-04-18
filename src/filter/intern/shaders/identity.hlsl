cbuffer params : register(b0) {
    uint level;
}

float4
main(float4 pos : SV_Position) : SV_Target {
    const uint size  = level * level;
    const uint idx = uint(pos.x) + uint(pos.y) * level * size;

    const uint r = idx % size;
    const uint g = (idx / size) % size;
    const uint b = idx / (size * size);

    return float4(float3(r, g, b) / float(size - 1u), 1.0);
}
