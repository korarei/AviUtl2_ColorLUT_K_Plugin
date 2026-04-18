static const float4 pos[3] = {
    float4(-1.0,  1.0, 0.0, 1.0),
    float4( 3.0,  1.0, 0.0, 1.0),
    float4(-1.0, -3.0, 0.0, 1.0)
};

float4
main(uint vertexID : SV_VertexID) : SV_Position {
    return pos[vertexID];
}
