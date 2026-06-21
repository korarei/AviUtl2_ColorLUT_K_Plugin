cbuffer params : register(b0) {
    uint level;
}

float4 main(float4 pos : SV_Position) : SV_Target {
    const uint size  = level * level;
    const uint idx = uint(pos.x) + uint(pos.y) * level * size;

    return float4(float3(idx % size, (idx / size) % size, idx / (size * size)) / float(size - 1u), 1.0);
}
