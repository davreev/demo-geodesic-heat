#include "viewer.hpp"

#include <dr/container_utils.hpp>

#include <dr/app/event_handlers.hpp>
#include <dr/app/gfx_utils.hpp>

#include "assets.hpp"
#include "viewer.h"

namespace dr
{
namespace
{

template <typename T>
struct DefaultResources;

template <>
struct DefaultResources<Viewer::ContourColorMaterial>
{
    static inline GfxPipeline pipeline;
    static inline GfxShader shader;
    struct
    {
        GfxImage image;
        GfxSampler sampler;
    } inline static matcap;

    static void init_shader()
    {
        ShaderAsset const* vs = get_asset(AssetHandle::Shader_ContourColorVert, true);
        assert(vs);

        ShaderAsset const* fs = get_asset(AssetHandle::Shader_ContourColorFrag, true);
        assert(fs);

        shader.init(contour_color_shader_desc(vs->src.c_str(), fs->src.c_str()));
        assert(shader.is_valid());
    };

    static void init()
    {
        assert(!pipeline.is_valid());

        shader = GfxShader::alloc();
        init_shader();

        pipeline = GfxPipeline::make(contour_color_pipeline_desc(shader));
        assert(pipeline.is_valid());

        {
            ImageAsset const* image = get_asset(AssetHandle::Image_Matcap);
            assert(image);

            matcap.image = GfxImage::make(
                contour_color_matcap_image_desc(image->data.get(), image->width, image->height));
            assert(matcap.image.is_valid());

            matcap.sampler = GfxSampler::make(contour_color_matcap_sampler_desc());
            assert(matcap.sampler.is_valid());
        }
    };
};

template <>
struct DefaultResources<Viewer::ContourLineMaterial>
{
    inline static GfxPipeline pipeline;
    inline static GfxShader shader;

    static void init_shader()
    {
        ShaderAsset const* vs = get_asset(AssetHandle::Shader_ContourLineVert, true);
        assert(vs);

        ShaderAsset const* fs = get_asset(AssetHandle::Shader_ContourLineFrag, true);
        assert(fs);

        shader.init(contour_line_shader_desc(vs->src.c_str(), fs->src.c_str()));
        assert(shader.is_valid());
    };

    static void init()
    {
        assert(!pipeline.is_valid());

        shader = GfxShader::alloc();
        init_shader();

        pipeline = GfxPipeline::make(contour_line_pipeline_desc(shader));
        assert(pipeline.is_valid());
    };
};

// Returns the given handle if it's valid. Otherwise, returns the given default.
template <typename Handle>
Handle const valid_or(Handle const handle, Handle const other)
{
    return (handle.id == SG_INVALID_ID) ? other : handle;
}

template <typename T>
sg_range to_range(Span<T> const& span)
{
    return {span.data(), span.size() * sizeof(T)};
}

template <typename Resource>
void init_resource(Resource& buf, typename Resource::Desc const& desc)
{
    if (buf.is_valid())
        buf.init(desc);
    else
        buf = Resource::make(desc);
}

void set_pipeline(Viewer::DrawContext& ctx, GfxPipeline::Handle const pipeline)
{
    // Avoid unecessary pipeline change
    if (pipeline.id == ctx.pipeline.id)
        return;

    sg_apply_pipeline(pipeline);
    ctx.pipeline = pipeline;
    ctx.bindings = {};
    ctx.material = nullptr;
    ctx.geometry = nullptr;
}

void set_material(Viewer::DrawContext& ctx, Viewer::ContourColorMaterial const* material)
{
    using Default = DefaultResources<Viewer::ContourColorMaterial>;
    set_pipeline(ctx, valid_or(material->pipeline, Default::pipeline.handle()));

    ctx.bindings.images[0] = valid_or(material->matcap.image, Default::matcap.image.handle());
    ctx.bindings.samplers[0] = valid_or(material->matcap.sampler, Default::matcap.sampler.handle());

    struct
    {
        f32 spacing;
        f32 offset;
    } u;

    u.spacing = material->spacing;
    u.offset = material->offset;
    sg_apply_uniforms(UniformBlock_Material, {&u, sizeof(u)});

    ctx.material = material;
}

void set_material(Viewer::DrawContext& ctx, Viewer::ContourLineMaterial const* material)
{
    using Default = DefaultResources<Viewer::ContourLineMaterial>;
    set_pipeline(ctx, valid_or(material->pipeline, Default::pipeline.handle()));

    struct
    {
        f32 spacing;
        f32 line_width;
        f32 offset;
    } u;

    u.spacing = material->spacing;
    u.line_width = material->line_width;
    u.offset = material->offset;
    sg_apply_uniforms(UniformBlock_Material, {&u, sizeof(u)});

    ctx.material = material;
}

template <typename Material>
void set_geometry(Viewer::DrawContext& ctx, Viewer::MeshPlotGeometry const* geometry)
{
    using OkMaterials = TypePack<Viewer::ContourColorMaterial, Viewer::ContourLineMaterial>;

    // NOTE(dr): Can static dispatch based on bound material type
    static_assert(
        OkMaterials::includes<Material>,
        "Geometry isn't compatible with bound material type");

    ctx.bindings.vertex_buffers[0] = geometry->mesh->vertices.buffer;
    ctx.bindings.vertex_buffers[1] = geometry->mesh->vertices.buffer;
    ctx.bindings.vertex_buffer_offsets[1] = geometry->mesh->vertices.count * sizeof(f32[3]);
    ctx.bindings.vertex_buffers[2] = geometry->scalars.buffer;
    ctx.bindings.index_buffer = geometry->mesh->indices.buffer;

    ctx.geometry = geometry;
}

template <typename Material>
void submit_draw(Viewer::DrawContext const& ctx, Viewer::MeshPlot const& object)
{
    using OkMaterials = TypePack<Viewer::ContourColorMaterial, Viewer::ContourLineMaterial>;

    // NOTE(dr): Can static dispatch based on bound material type
    static_assert(
        OkMaterials::includes<Material>,
        "Object isn't compatible with bound material type");

    struct
    {
        f32 local_to_clip[16];
        f32 local_to_view[16];
    } u;

    Mat4<f32> const local_to_world = object.transform.to_matrix();
    as_mat<4, 4>(u.local_to_clip) = ctx.transforms.world_to_clip * local_to_world;
    as_mat<4, 4>(u.local_to_view) = ctx.transforms.world_to_view * local_to_world;
    sg_apply_uniforms(UniformBlock_Object, {&u, sizeof(u)});

    const isize num_indices = object.geometry->mesh->indices.count;
    sg_draw(0, num_indices, 1);
}

template <int material_id, typename Object>
void draw_impl(Viewer::DrawContext& ctx, Object const& object)
{
    using Material = typename Object::Materials::template At<material_id>;

    auto mat = std::get<material_id>(object.materials);
    if (mat == nullptr)
        return;

    auto geom = object.geometry;
    if (geom == nullptr)
        return;

    bool bindings_dirty = false;

    // Update material
    if (mat != ctx.material)
    {
        set_material(ctx, mat);
        bindings_dirty = true;
    }

    // Update geometry
    if (geom != ctx.geometry)
    {
        set_geometry<Material>(ctx, geom);
        bindings_dirty = true;
    }

    // Commit bound resources
    if (bindings_dirty)
        sg_apply_bindings(ctx.bindings);

    // Submit draw call
    submit_draw<Material>(ctx, object);
}

} // namespace

void Viewer::init_default_resources()
{
    DefaultResources<Viewer::ContourColorMaterial>::init();
    DefaultResources<Viewer::ContourLineMaterial>::init();
}

void Viewer::reload_default_shaders()
{
    DefaultResources<Viewer::ContourColorMaterial>::init_shader();
    DefaultResources<Viewer::ContourLineMaterial>::init_shader();
    // ...
}

template <>
void Viewer::DrawContext::draw<0>(Viewer::MeshPlot const& object)
{
    draw_impl<0>(*this, object);
}

template <>
void Viewer::DrawContext::draw<1>(Viewer::MeshPlot const& object)
{
    draw_impl<1>(*this, object);
}

Viewer::DrawContext Viewer::make_draw_context(
    Mat4<f32> const& world_to_view,
    Mat4<f32> const& view_to_clip)
{
    DrawContext ctx{};
    ctx.transforms.view_to_clip = view_to_clip;
    ctx.transforms.world_to_view = world_to_view;
    ctx.transforms.world_to_clip = view_to_clip * world_to_view;
    return ctx;
}

GfxPipeline Viewer::ContourColorMaterial::make_pipeline(GfxShader::Handle shader)
{
    return GfxPipeline::make(contour_color_pipeline_desc(shader));
}

GfxPipeline Viewer::ContourLineMaterial::make_pipeline(GfxShader::Handle shader)
{
    return GfxPipeline::make(contour_line_pipeline_desc(shader));
}

void Viewer::MeshGeometry::set_vertices(
    Span<Vec3<f32> const> const& positions,
    Span<Vec3<f32> const> const& normals)
{
    assert(positions.size() == normals.size());

    vertices.count = positions.size();
    if (vertices.count > vertices.capacity)
    {
        init_resource(vertices.buffer, mesh_vertex_buffer_desc(vertices.size()));
        vertices.capacity = vertices.count;
    }

    sg_append_buffer(vertices.buffer, to_range(positions));
    sg_append_buffer(vertices.buffer, to_range(normals));
}

void Viewer::MeshGeometry::set_indices(Span<Vec3<i32> const> const& faces)
{
    indices.count = faces.size() * 3;
    if (indices.count > indices.capacity)
    {
        init_resource(indices.buffer, mesh_index_buffer_desc(indices.size()));
        indices.capacity = indices.count;
    }

    sg_update_buffer(indices.buffer, to_range(faces));
}

void Viewer::MeshPlotGeometry::set_scalars(Span<f32 const> const& values)
{
    assert(mesh != nullptr);
    assert(values.size() == mesh->vertices.count);

    scalars.count = values.size();
    if (scalars.count > scalars.capacity)
    {
        init_resource(scalars.buffer, mesh_plot_buffer_desc(scalars.size()));
        scalars.capacity = scalars.count;
    }

    sg_update_buffer(scalars.buffer, to_range(values));
}

} // namespace dr