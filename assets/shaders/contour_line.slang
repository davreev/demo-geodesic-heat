import "modules/common";
import "param_types";

struct MaterialParams
{
    float spacing;
    float line_width;
    float offset;
}

ConstantBuffer<PassParams> pass;
ConstantBuffer<MaterialParams> material;
ConstantBuffer<ObjectParams> object;

static float4x4 local_to_view = mul(pass.world_to_view, object.local_to_world);
static float4x4 local_to_clip = mul(pass.world_to_clip, object.local_to_world);

struct Vertex
{
    float3 position;
    float3 normal;
    float scalar;
}

// NOTE(dr): Interface parameters must be passed as args to avoid name mangling by slangc. This is
// necessary since GLSL 330/GLSL ES 300 require matching interface parameter names between shader
// stages.

[shader("vertex")]
float4 vertex_main(Vertex v, out float3 view_position, out float3 view_normal, out float scalar) :
    SV_Position
{
    view_position = mul(local_to_view, { v.position, 1.0 }).xyz;
    // NOTE(dr): This assumes local_to_view has uniform scaling
    view_normal = mul(float3x3(local_to_view), v.normal);
    scalar = v.scalar;
    return mul(local_to_clip, { v.position, 1.0 });
}

float4 shade_rim(float3 view_dir, float3 view_norm, float4 color, float4 color_base)
{
    const float t = 1.0 - abs(dot(view_dir, view_norm));
    return lerp(color_base, color, t * t * t);
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
    const float f = (scalar + material.offset) / material.spacing;

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
