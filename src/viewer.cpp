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

// Specialize for valid instance/material permutations
template <typename Material, typename Instance>
Material const* instance_material(Instance const&);

template <>
Viewer::ContourColorMaterial const* instance_material(Viewer::MeshPlotInstance const& inst)
{
    return inst.materials.contour_color;
}

template <>
Viewer::ContourLineMaterial const* instance_material(Viewer::MeshPlotInstance const& inst)
{
    return inst.materials.contour_line;
}

// Specialize for valid instance/geometry permutations
template <typename Geometry, typename Instance>
Geometry const* instance_geometry(Instance const&);

template <>
Viewer::MeshPlotGeometry const* instance_geometry(Viewer::MeshPlotInstance const& inst)
{
    return inst.geometry;
}

struct DrawContext
{
    Viewer::Frame const* frame{};
    GfxPipeline::Handle pipeline{};
    sg_bindings bindings{};

    DrawContext(Viewer const& viewer) : frame{&viewer.frame} {}

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

    void bind_resources(Viewer::ContourColorMaterial const& mat)
    {
        using Default = DefaultResources<Viewer::ContourColorMaterial>;
        bindings.images[0] = valid_or(mat.matcap.image, Default::matcap.image.handle());
        bindings.samplers[0] = valid_or(mat.matcap.sampler, Default::matcap.sampler.handle());
    }

    void bind_resources(Viewer::ContourLineMaterial const&)
    {
        // ...
    }

    void bind_resources(Viewer::MeshPlotGeometry const& geom)
    {
        bindings.vertex_buffers[0] = geom.mesh->vertices.buffer;
        bindings.vertex_buffers[1] = geom.mesh->vertices.buffer;
        bindings.vertex_buffer_offsets[1] = geom.mesh->vertices.count * sizeof(f32[3]);
        bindings.vertex_buffers[2] = geom.scalars.buffer;
        bindings.index_buffer = geom.mesh->indices.buffer;
    }

    void apply_uniforms(Viewer::ContourColorMaterial const& mat)
    {
        struct
        {
            float spacing;
            float offset;
        } u;

        u.spacing = mat.spacing;
        u.offset = mat.offset;
        sg_apply_uniforms(UniformBlock_Material, {&u, sizeof(u)});
    }

    void apply_uniforms(Viewer::ContourLineMaterial const& mat)
    {
        struct
        {
            float spacing;
            float line_width;
            float offset;
        } u;

        u.spacing = mat.spacing;
        u.line_width = mat.line_width;
        u.offset = mat.offset;
        sg_apply_uniforms(UniformBlock_Material, {&u, sizeof(u)});
    }

    void apply_uniforms(Viewer::MeshPlotGeometry const&)
    {
        // ...
    }

    void draw(Viewer::MeshPlotInstance const& inst)
    {
        // Update instance uniforms
        {
            Mat4<f32> const local_to_world = inst.transform.to_matrix();

            struct
            {
                f32 local_to_clip[16];
                f32 local_to_view[16];
            } u;

            as_mat<4, 4>(u.local_to_clip) = frame->world_to_clip * local_to_world;
            as_mat<4, 4>(u.local_to_view) = frame->world_to_view * local_to_world;
            sg_apply_uniforms(UniformBlock_Instance, {&u, sizeof(u)});
        }

        const isize num_indices = inst.geometry->mesh->indices.count;
        sg_draw(0, num_indices, 1);
    }
};

template <typename Material, typename Geometry, typename Instance>
void draw_impl(DrawContext ctx, Span<Instance const> instances)
{
    Material const* prev_mat{};
    Geometry const* prev_geom{};

    for (Instance const& inst : instances)
    {
        Material const* mat = instance_material<Material>(inst);
        if (mat == nullptr)
            continue;

        Geometry const* geom = instance_geometry<Geometry>(inst);
        if (geom == nullptr)
            continue;

        bool pipeline_changed = false;
        bool bindings_dirty = false;

        // Update material
        if (mat != prev_mat)
        {
            pipeline_changed = ctx.apply_pipeline(*mat);
            ctx.bind_resources(*mat), bindings_dirty = true;
            ctx.apply_uniforms(*mat);
            prev_mat = mat;
        }

        // Update geometry
        if (geom != prev_geom || pipeline_changed)
        {
            ctx.bind_resources(*geom), bindings_dirty = true;
            ctx.apply_uniforms(*geom);
            prev_geom = geom;
        }

        // Commit bound resources
        if (bindings_dirty)
            ctx.apply_bindings();

        // Draw instance
        ctx.draw(inst);
    }
}

template <typename T>
sg_range to_range(Span<T> const& span)
{
    return {span.data(), span.size() * sizeof(T)};
}

void init_buffer(GfxBuffer& buf, GfxBuffer::Desc const& desc)
{
    if (buf.is_valid())
        buf.init(desc);
    else
        buf = GfxBuffer::make(desc);
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

void Viewer::update()
{
    f64 const dt_s = App::delta_time_s();

    // Update view
    {
        auto& ctrl = view.controls;
        auto& cam = view.camera;

        f32 const t = saturate(ctrl.sensitivity * dt_s);

        ctrl.orbit.update(t);
        ctrl.orbit.apply(cam);

        ctrl.zoom.update(t);
        ctrl.zoom.apply(cam);

        ctrl.pan.update(t);
        ctrl.pan.apply(cam);

        cam.pivot.position += (view.target.position - cam.pivot.position) * t;
    }

    // Update frame state
    {
        frame.view_to_clip = make_perspective<NdcType_OpenGl>(
            view.frustum.fov_y,
            App::aspect(),
            view.frustum.clip_near,
            view.frustum.clip_far);

        frame.world_to_view = view.camera.transform().inverse_to_matrix();
        frame.world_to_clip = frame.view_to_clip * frame.world_to_view;
    }
}

template <>
void Viewer::draw<Viewer::ContourColorMaterial, Viewer::MeshPlotGeometry>(
    Span<MeshPlotInstance const> const& instances) const
{
    draw_impl<ContourColorMaterial, MeshPlotGeometry>({*this}, instances);
}

template <>
void Viewer::draw<Viewer::ContourLineMaterial, Viewer::MeshPlotGeometry>(
    Span<MeshPlotInstance const> const& instances) const
{
    draw_impl<ContourLineMaterial, MeshPlotGeometry>({*this}, instances);
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

void Viewer::MeshGeometry::set_vertices(
    Span<Vec3<f32> const> const& positions,
    Span<Vec3<f32> const> const& normals)
{
    assert(positions.size() == normals.size());

    vertices.count = positions.size();
    if (vertices.count > vertices.capacity)
    {
        init_buffer(vertices.buffer, mesh_vertex_buffer_desc(vertices.size()));
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
        init_buffer(indices.buffer, mesh_index_buffer_desc(indices.size()));
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
        init_buffer(scalars.buffer, mesh_plot_buffer_desc(scalars.size()));
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

void Viewer::View::frame_target()
{
    controls.zoom.target.distance = target.radius / std::sin(frustum.fov_y * 0.5);
    controls.pan.target.offset = {};
}

} // namespace dr