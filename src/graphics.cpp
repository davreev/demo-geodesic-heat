#include "graphics.hpp"

#include <cassert>

#include "assets.hpp"
#include "graphics.h"

namespace dr
{
namespace
{

// clang-format off
struct {
    struct {
        struct {
            GfxPipeline pipeline;
            GfxShader shader;
        } contour_color;
        struct {
            GfxPipeline pipeline;
            GfxShader shader;
        } contour_line;
    } materials;

    struct {
        GfxImage matcap;
        // ...
    } images;

    struct {
        GfxSampler matcap;
        // ...
    } samplers;
} state;
// clang-format on

template <typename Material>
void init_shader();

template <>
void init_shader<ContourColor>()
{
    auto& mat = state.materials.contour_color;
    if (!mat.shader.is_valid())
        mat.shader = GfxShader::alloc();

    ShaderAsset const* vert = get_asset(AssetHandle::Shader_ContourColorVert, true);
    assert(vert);

    ShaderAsset const* frag = get_asset(AssetHandle::Shader_ContourColorFrag, true);
    assert(frag);

    mat.shader.init(contour_color_shader_desc(vert->src.c_str(), frag->src.c_str()));
}

template <>
void init_shader<ContourLine>()
{
    auto& mat = state.materials.contour_line;
    if (!mat.shader.is_valid())
        mat.shader = GfxShader::alloc();

    ShaderAsset const* vert = get_asset(AssetHandle::Shader_ContourLineVert, true);
    assert(vert);

    ShaderAsset const* frag = get_asset(AssetHandle::Shader_ContourLineFrag, true);
    assert(frag);

    mat.shader.init(contour_line_shader_desc(vert->src.c_str(), frag->src.c_str()));
}

template <typename Material>
void init_material();

template <>
void init_material<ContourColor>()
{
    auto& mat = state.materials.contour_color;
    assert(!mat.pipeline.is_valid());

    init_shader<ContourColor>();
    mat.pipeline = GfxPipeline::make(contour_color_pipeline_desc(mat.shader));
}

template <>
void init_material<ContourLine>()
{
    auto& mat = state.materials.contour_line;
    assert(!mat.pipeline.is_valid());

    init_shader<ContourLine>();
    mat.pipeline = GfxPipeline::make(contour_line_pipeline_desc(mat.shader));
}

template <typename T>
sg_range to_range(Span<T> const& span)
{
    return {span.data(), span.size() * sizeof(T)};
}

void update_buffer(GfxBuffer& buf, GfxBuffer::Desc const& desc)
{
    if (buf.is_valid())
        buf.init(desc);
    else
        buf = GfxBuffer::make(desc);
}

template <typename Material>
void apply_uniforms(Material&& mat)
{
    auto const& [vert, frag] = mat.uniforms;
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, {&vert, sizeof(vert)});
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, {&frag, sizeof(frag)});
}

} // namespace

void init_graphics()
{
    // Initialize materials
    init_material<ContourColor>();
    init_material<ContourLine>();
    // ...

    // Initialize shared resources
    {
        ImageAsset const* image = get_asset(AssetHandle::Image_Matcap);
        state.images.matcap = GfxImage::make(
            matcap_image_desc(image->data.get(), image->width, image->height));
    }

    if (!state.samplers.matcap.is_valid())
        state.samplers.matcap = GfxSampler::make(matcap_sampler_desc());
}

void reload_shaders()
{
    init_shader<ContourColor>();
    init_shader<ContourLine>();
    // ...
}

////////////////////////////////////////////////////////////////////////////////
// RenderMesh

void RenderMesh::set_vertex_capacity(isize const value)
{
    update_buffer(vertices[0], vertex_buffer_desc(value * sizeof(f32[6])));
    update_buffer(vertices[1], vertex_buffer_desc(value * sizeof(f32)));
    vertex_capacity = value;
}

void RenderMesh::set_index_capacity(isize const value)
{
    update_buffer(indices, index_buffer_desc(value * sizeof(i32)));
    index_capacity = value;
}

void RenderMesh::set_vertices(
    Span<Vec3<f32> const> const& positions,
    Span<Vec3<f32> const> const& normals)
{
    assert(positions.size() == normals.size());

    vertex_count = positions.size();
    if (vertex_count > vertex_capacity)
        set_vertex_capacity(vertex_count);

    sg_append_buffer(vertices[0], to_range(positions));
    sg_append_buffer(vertices[0], to_range(normals));
}

void RenderMesh::set_vertices(Span<f32 const> const& scalars)
{
    vertex_count = scalars.size();
    if (vertex_count > vertex_capacity)
        set_vertex_capacity(vertex_count);

    sg_update_buffer(vertices[1], to_range(scalars));
}

void RenderMesh::set_indices(Span<Vec3<i32> const> const& faces)
{
    index_count = faces.size() * 3;
    if (index_count > index_capacity)
        set_index_capacity(index_count);

    sg_update_buffer(indices, to_range(faces));
}

void RenderMesh::bind_resources(sg_bindings& dst) const
{
    dst.vertex_buffers[0] = vertices[0];
    dst.vertex_buffers[1] = vertices[0];
    dst.vertex_buffer_offsets[1] = vertex_count * sizeof(f32[3]);
    dst.vertex_buffers[2] = vertices[1];
    dst.index_buffer = indices;
}

////////////////////////////////////////////////////////////////////////////////
// ContourColor

GfxPipeline::Handle ContourColor::pipeline() { return state.materials.contour_color.pipeline; }

void ContourColor::bind_resources(sg_bindings& dst) const
{
    dst.fs.images[0] = state.images.matcap;
    dst.fs.samplers[0] = state.samplers.matcap;
}

void ContourColor::apply_uniforms() const { dr::apply_uniforms(*this); }

////////////////////////////////////////////////////////////////////////////////
// ContourLine

GfxPipeline::Handle ContourLine::pipeline() { return state.materials.contour_line.pipeline; }

void ContourLine::bind_resources(sg_bindings& /*dst*/) const {}

void ContourLine::apply_uniforms() const { dr::apply_uniforms(*this); }

} // namespace dr