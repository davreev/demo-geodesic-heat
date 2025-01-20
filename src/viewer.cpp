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
struct Default;

template <>
struct Default<Viewer::ContourColorMaterial>
{
    GfxPipeline pipeline;
    GfxShader shader;
    struct
    {
        GfxImage image;
        GfxSampler sampler;
    } matcap;

    void init_shader()
    {
        ShaderAsset const* vs = get_asset(AssetHandle::Shader_ContourColorVert, true);
        assert(vs);

        ShaderAsset const* fs = get_asset(AssetHandle::Shader_ContourColorFrag, true);
        assert(fs);

        shader.init(contour_color_shader_desc(vs->src.c_str(), fs->src.c_str()));
        assert(shader.is_valid());
    };

    void init()
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

    static Default& get()
    {
        static Default instance{};
        return instance;
    }
};

template <>
struct Default<Viewer::ContourLineMaterial>
{
    GfxPipeline pipeline;
    GfxShader shader;

    void init_shader()
    {
        ShaderAsset const* vs = get_asset(AssetHandle::Shader_ContourLineVert, true);
        assert(vs);

        ShaderAsset const* fs = get_asset(AssetHandle::Shader_ContourLineFrag, true);
        assert(fs);

        shader.init(contour_line_shader_desc(vs->src.c_str(), fs->src.c_str()));
        assert(shader.is_valid());
    };

    void init()
    {
        assert(!pipeline.is_valid());

        shader = GfxShader::alloc();
        init_shader();

        pipeline = GfxPipeline::make(contour_line_pipeline_desc(shader));
        assert(pipeline.is_valid());
    };

    static Default& get()
    {
        static Default instance{};
        return instance;
    }
};

void reload_default_shaders()
{
    Default<Viewer::ContourColorMaterial>::get().init_shader();
    Default<Viewer::ContourLineMaterial>::get().init_shader();
    // ...
}

// Returns the given handle if it's valid. Otherwise, returns a handle to the given default
// resource.
template <typename Handle, typename Resource>
Handle const valid_or(Handle const handle, Resource const& resource)
{
    return (handle.id == SG_INVALID_ID) ? resource.handle() : handle;
}

template <typename T>
struct Tag
{
};

template <typename Instance = void>
struct DrawContext
{
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

    void apply_bindings() { sg_apply_bindings(bindings); }
};

template <>
struct DrawContext<Viewer::MeshPlotInstance> : DrawContext<>
{
    using Instance = Viewer::MeshPlotInstance;

    Span<Instance const> instances{};
    Viewer::Frame const* frame{};

    DrawContext(Viewer const& viewer) :
        instances{as_span(viewer.mesh_plot_instances)}, frame{&viewer.frame}
    {
    }

    auto material(Instance const& inst, Tag<Viewer::ContourColorMaterial>) const
    {
        return inst.contour_color;
    }

    auto material(Instance const& inst, Tag<Viewer::ContourLineMaterial>) const
    {
        return inst.contour_line;
    }

    auto geometry(Instance const& inst, Tag<Viewer::MeshPlotGeometry>) const
    {
        return inst.mesh_plot;
    }

    bool apply_pipeline(Viewer::ContourColorMaterial const& mat)
    {
        auto const& def = Default<Viewer::ContourColorMaterial>::get();
        return DrawContext<>::apply_pipeline(valid_or(mat.pipeline, def.pipeline));
    }

    bool apply_pipeline(Viewer::ContourLineMaterial const& mat)
    {
        auto const& def = Default<Viewer::ContourLineMaterial>::get();
        return DrawContext<>::apply_pipeline(valid_or(mat.pipeline, def.pipeline));
    }

    void bind_resources(Viewer::ContourColorMaterial const& mat)
    {
        auto const& def = Default<Viewer::ContourColorMaterial>::get();
        bindings.images[0] = valid_or(mat.matcap.image, def.matcap.image);
        bindings.samplers[0] = valid_or(mat.matcap.sampler, def.matcap.sampler);
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
            float width;
            float offset;
        } u;

        u.spacing = mat.spacing;
        u.width = mat.width;
        u.offset = mat.offset;
        sg_apply_uniforms(UniformBlock_Material, {&u, sizeof(u)});
    }

    void apply_uniforms(Viewer::MeshPlotGeometry const&)
    {
        // ...
    }

    void draw(Instance const& inst)
    {
        // Update instance uniform block
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

        const isize num_indices = inst.mesh_plot->mesh->indices.count;
        sg_draw(0, num_indices, 1);
    }
};

template <typename Material, typename Geometry, typename Instance>
void draw_impl(DrawContext<Instance> ctx)
{
    Material const* prev_mat{};
    Geometry const* prev_geom{};

    for (Instance const& inst : ctx.instances)
    {
        Material const* mat = ctx.material(inst, Tag<Material>{});
        if (mat == nullptr)
            continue;

        Geometry const* geom = ctx.geometry(inst, Tag<Geometry>{});
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
    Default<Viewer::ContourColorMaterial>::get().init();
    Default<Viewer::ContourLineMaterial>::get().init();
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

void Viewer::draw() const
{
    // TODO(dr): Sort instances by {material index, geometry index} to minimize state changes

    // Color contour plots
    draw_impl<Viewer::ContourColorMaterial, Viewer::MeshPlotGeometry>(
        DrawContext<Viewer::MeshPlotInstance>{*this});

    // Line contour plots
    draw_impl<Viewer::ContourLineMaterial, Viewer::MeshPlotGeometry>(
        DrawContext<Viewer::MeshPlotInstance>{*this});
}

void Viewer::handle_event(App::Event const& event)
{
    // Camera controls
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

    switch (event.type)
    {
        case SAPP_EVENTTYPE_KEY_DOWN:
        {
            switch (event.key_code)
            {
                case SAPP_KEYCODE_F:
                {
                    if (is_mouse_over(event))
                        view.frame_target();

                    break;
                };
                case SAPP_KEYCODE_R:
                {
                    if (is_mouse_over(event))
                        reload_default_shaders();

                    break;
                };
                default:
                {
                    // ...
                }
            }
            break;
        }
        default:
        {
            // ...
        }
    }
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