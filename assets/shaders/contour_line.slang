import "modules/common.slang";

struct Material
{
    float spacing;
    float line_width;
    float offset;
}
ConstantBuffer<Material> material;

struct Instance
{
    float4x4 local_to_clip;
    float4x4 local_to_view;
}
ConstantBuffer<Instance> instance;

struct Vertex
{
    float3 position;
    float3 normal;
    float scalar;
}

[shader("vertex")]
float4 vertex_main(Vertex v, out float3 view_position, out float3 view_normal, out float scalar) :
    SV_Position
{
    view_position = mul(instance.local_to_view, { v.position, 1.0 }).xyz;
    // NOTE(dr): This assumes local_to_view has uniform scaling
    view_normal = mul(float3x3(instance.local_to_view), v.normal);
    scalar = v.scalar;
    return mul(instance.local_to_clip, { v.position, 1.0 });
}

float4 shade_rim(float3 view_dir, float3 view_norm, float4 color, float4 color_base)
{
    float t = 1.0 - abs(dot(view_dir, view_norm));
    return lerp(color_base, color, t * t * t);
}

float4 draw_contour(float f, float width, float4 color, float4 color_base)
{
    float df = fwidth(f);
    float t = abs(frac(f + 0.5) - 0.5);
    const float h = 0.5;
    return lerp(color, color_base, smoothstep((width - h) * df, (width + h) * df, t));
}

[shader("fragment")]
float4 fragment_main(
    float3 view_position,
    float3 view_normal,
    float scalar,
    bool is_front_face: SV_IsFrontFace) :
    SV_Target
{
    float4 col = { 0.85, 0.85, 0.85, 0.0 };
    col = shade_rim(normalize(view_position), view_normal, { 0.85, 0.85, 0.85, 0.2 }, col);
    float f = (scalar + material.offset) / material.spacing;

    // Minor lines
    col = draw_contour(
        4.0 * f,
        material.line_width,
        { 0.85, 0.85, 0.85, is_front_face ? 0.1 : 0.5 },
        col);

    // Major lines
    col = draw_contour(
        f,
        1.5 * material.line_width,
        float4( { 0.85 }, is_front_face ? 0.2 : 1.0),
        col);

    return col;
}
