#include "viewer.hpp"

#include <type_traits>

#include <dr/container_utils.hpp>
#include <dr/meta.hpp>

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
[[maybe_unused]]
constexpr bool always_false{false};

template <typename Material>
Material const* get_material(Viewer::MeshPlot const& object)
{
    if constexpr (std::is_same_v<Material, Viewer::ContourColorMaterial>)
        return object.materials.contour_color;
    else if constexpr (std::is_same_v<Material, Viewer::ContourLineMaterial>)
        return object.materials.contour_line;
    else
        static_assert(always_false<Material>, "Material not available on object");
}

struct DrawContext
{
    struct
    {
        Mat4<f32> view_to_clip;
        Mat4<f32> world_to_view;
        Mat4<f32> world_to_clip;
    } transforms;

    GfxPipeline::Handle pipeline{};
    sg_bindings bindings{};

    bool apply_pipeline(GfxPipeline::Handle const pipeline)
    {
        // Avoid unecessary pipeline state change
        if (pipeline.id != this->pipeline.id)
        {
            sg_apply_pipeline(pipeline);
            this->pipeline = pipeline;
            bindings = {};
            return true;
        }
        return false;
    }

    template <typename Material>
    bool apply_pipeline(Material const& mat)
    {
        using Default = DefaultResources<Material>;
        return apply_pipeline(valid_or(mat.pipeline, Default::pipeline.handle()));
    }

    void apply_bindings() { sg_apply_bindings(bindings); }
};

void bind_resources(Viewer::ContourColorMaterial const& mat, DrawContext& ctx)
{
    using Default = DefaultResources<Viewer::ContourColorMaterial>;
    ctx.bindings.images[0] = valid_or(mat.matcap.image, Default::matcap.image.handle());
    ctx.bindings.samplers[0] = valid_or(mat.matcap.sampler, Default::matcap.sampler.handle());
}

void apply_uniforms(Viewer::ContourColorMaterial const& mat, DrawContext const& /*ctx*/)
{
    struct
    {
        f32 spacing;
        f32 offset;
    } u;

    u.spacing = mat.spacing;
    u.offset = mat.offset;
    sg_apply_uniforms(UniformBlock_Material, {&u, sizeof(u)});
}

void bind_resources(Viewer::ContourLineMaterial const& /*mat*/, DrawContext& /*ctx*/)
{
    // ...
}

void apply_uniforms(Viewer::ContourLineMaterial const& mat, DrawContext const& /*ctx*/)
{
    struct
    {
        f32 spacing;
        f32 line_width;
        f32 offset;
    } u;

    u.spacing = mat.spacing;
    u.line_width = mat.line_width;
    u.offset = mat.offset;
    sg_apply_uniforms(UniformBlock_Material, {&u, sizeof(u)});
}

template <typename Material>
void bind_resources(Viewer::MeshPlotGeometry const& geom, DrawContext& ctx)
{
    using CompatMaterials = TypePack<Viewer::ContourColorMaterial, Viewer::ContourLineMaterial>;

    // NOTE(dr): Can static dispatch based on bound material type
    static_assert(
        CompatMaterials::includes<Material>,
        "Geometry isn't compatible with bound material type");

    ctx.bindings.vertex_buffers[0] = geom.mesh->vertices.buffer;
    ctx.bindings.vertex_buffers[1] = geom.mesh->vertices.buffer;
    ctx.bindings.vertex_buffer_offsets[1] = geom.mesh->vertices.count * sizeof(f32[3]);
    ctx.bindings.vertex_buffers[2] = geom.scalars.buffer;
    ctx.bindings.index_buffer = geom.mesh->indices.buffer;
}

template <typename Material>
void apply_uniforms(Viewer::MeshPlotGeometry const& /*geom*/, DrawContext const& /*ctx*/)
{
    using CompatMaterials = TypePack<Viewer::ContourColorMaterial, Viewer::ContourLineMaterial>;

    // NOTE(dr): Can static dispatch based on bound material type
    static_assert(
        CompatMaterials::includes<Material>,
        "Geometry isn't compatible with bound material type");

    // ...
}

template <typename Material>
void draw(Viewer::MeshPlot const& object, DrawContext const& ctx)
{
    using CompatMaterials = TypePack<Viewer::ContourColorMaterial, Viewer::ContourLineMaterial>;

    // NOTE(dr): Can static dispatch based on bound material type
    static_assert(
        CompatMaterials::includes<Material>,
        "Object isn't compatible with bound material type");

    // Update object uniforms
    {
        Mat4<f32> const local_to_world = object.transform.to_matrix();

        struct
        {
            f32 local_to_clip[16];
            f32 local_to_view[16];
        } u;

        as_mat<4, 4>(u.local_to_clip) = ctx.transforms.world_to_clip * local_to_world;
        as_mat<4, 4>(u.local_to_view) = ctx.transforms.world_to_view * local_to_world;
        sg_apply_uniforms(UniformBlock_Object, {&u, sizeof(u)});
    }

    const isize num_indices = object.geometry->mesh->indices.count;
    sg_draw(0, num_indices, 1);
}

template <typename Material, typename Object>
void draw_impl(DrawContext ctx, Span<Object const> objects)
{
    using Geometry = typename Object::Geometry;

    Material const* prev_mat{};
    Geometry const* prev_geom{};

    for (Object const& obj : objects)
    {
        Material const* mat = get_material<Material>(obj);
        if (mat == nullptr)
            continue;

        Geometry const* geom = obj.geometry;
        if (geom == nullptr)
            continue;

        bool pipeline_changed = false;
        bool bindings_dirty = false;

        // Update material
        if (mat != prev_mat)
        {
            pipeline_changed = ctx.apply_pipeline(*mat);
            bind_resources(*mat, ctx), bindings_dirty = true;
            apply_uniforms(*mat, ctx);
            prev_mat = mat;
        }

        // Update geometry
        if (geom != prev_geom || pipeline_changed)
        {
            bind_resources<Material>(*geom, ctx), bindings_dirty = true;
            apply_uniforms<Material>(*geom, ctx);
            prev_geom = geom;
        }

        // Commit bound resources
        if (bindings_dirty)
            ctx.apply_bindings();

        // Draw object
        draw<Material>(obj, ctx);
    }
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

DrawContext make_draw_context(Viewer::View const& view)
{
    DrawContext ctx{};
    ctx.transforms.view_to_clip = view.transforms.view_to_clip;
    ctx.transforms.world_to_view = view.transforms.world_to_view;
    ctx.transforms.world_to_clip = view.transforms.world_to_clip;
    return ctx;
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

void Viewer::update() { view.update(); }

template <>
void Viewer::draw<Viewer::ContourColorMaterial>(Span<MeshPlot const> const& objects) const
{
    draw_impl<ContourColorMaterial>(make_draw_context(view), objects);
}

template <>
void Viewer::draw<Viewer::ContourLineMaterial>(Span<MeshPlot const> const& objects) const
{
    draw_impl<ContourLineMaterial>(make_draw_context(view), objects);
}

void Viewer::handle_event(App::Event const& event)
{
    f32 const screen_to_view = dr::screen_to_view(view.frustum.fov_y, sapp_heightf());
    auto& ctrl = view.controls;

    camera_handle_mouse_event(
        event,
        ctrl.zoom.target,
        &ctrl.orbit.target,
        &ctrl.pan.target,
        screen_to_view,
        input.mouse_down);

    camera_handle_touch_event(
        event,
        ctrl.zoom.target,
        &ctrl.orbit.target,
        &ctrl.pan.target,
        screen_to_view,
        input.last_touch_points,
        input.last_num_touches);
}

template <>
GfxPipeline Viewer::make_material_pipeline<Viewer::ContourColorMaterial>(GfxShader::Handle shader)
{
    return GfxPipeline::make(contour_color_pipeline_desc(shader));
}

template <>
GfxPipeline Viewer::make_material_pipeline<Viewer::ContourLineMaterial>(GfxShader::Handle shader)
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

Viewer::View::View()
{
    controls.orbit.apply(camera);
    controls.zoom.apply(camera);
    controls.pan.apply(camera);
}

void Viewer::View::update()
{
    f64 const dt_s = App::delta_time_s();

    // Update and apply controls
    {
        f32 const t = saturate(controls.sensitivity * dt_s);

        controls.orbit.update(t);
        controls.orbit.apply(camera);

        controls.zoom.update(t);
        controls.zoom.apply(camera);

        controls.pan.update(t);
        controls.pan.apply(camera);

        camera.pivot.position += (target.position - camera.pivot.position) * t;
    }

    // Update transforms
    {
        transforms.view_to_clip = make_perspective<NdcType_OpenGl>(
            frustum.fov_y,
            App::aspect(),
            frustum.clip_near,
            frustum.clip_far);

        transforms.world_to_view = camera.transform().inverse_to_matrix();
        transforms.world_to_clip = transforms.view_to_clip * transforms.world_to_view;
    }
}

void Viewer::View::frame_target()
{
    controls.zoom.target.distance = target.radius / std::sin(frustum.fov_y * 0.5);
    controls.pan.target.offset = {};
}

} // namespace dr