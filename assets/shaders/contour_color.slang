import "modules/common";
import "param_types";

struct MaterialParams
{
    float spacing;
    float offset;
}

ConstantBuffer<PassParams> pass;
ConstantBuffer<MaterialParams> material;
ConstantBuffer<ObjectParams> object;

static float4x4 local_to_view = mul(pass.world_to_view, object.local_to_world);
static float4x4 local_to_clip = mul(pass.world_to_clip, object.local_to_world);

Sampler2D matcap;

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
float4 vertex_main(Vertex v, out float3 view_normal, out float scalar) : SV_Position
{
    // NOTE(dr): This assumes local_to_view has uniform scaling
    view_normal = normalize(mul(float3x3(local_to_view), v.normal));
    scalar = v.scalar;
    return mul(local_to_clip, { v.position, 1.0 });
}

[shader("fragment")]
float4 fragment_main(float3 view_normal, float scalar, bool is_front_face: SV_IsFrontFace) :
    SV_Target
{
    const float t = fract((scalar + material.offset) / material.spacing);
    const float3 view_norm = normalize(view_normal);
    float4 col = { t, view_norm.x * 0.5 + 0.5, 0.5, 1.0 };

    apply_matcap_shading(matcap, view_norm, 1.0, 0.9, col);
    if (is_front_face)
        col.rgb *= 0.2;

    return col;
}
