struct VSOut
{
    float4 pos : SV_Position;
};

VSOut SceneDepthVS(uint vertexId : SV_VertexID)
{
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    VSOut output;
    output.pos = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}
