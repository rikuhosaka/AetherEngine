struct PSIn
{
    float4 pos : SV_Position;
};

Texture2D<float> g_sceneDepth : register(t0);

float4 SceneDepthPS(PSIn input) : SV_Target
{
    int2 pixel = int2(input.pos.xy);
    float depth = g_sceneDepth.Load(int3(pixel, 0));
    return float4(depth, depth, depth, 1.0f);
}
