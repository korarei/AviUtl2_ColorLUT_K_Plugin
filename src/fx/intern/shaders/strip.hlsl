cbuffer params : register(b0) {
    uint size;
}

float4 main(float4 pos : SV_Position) : SV_Target {
    const uint idx = uint(pos.x);

    return float4(float3(idx % size, uint(pos.y), idx / size) / float(size - 1u), 1.0);
}
