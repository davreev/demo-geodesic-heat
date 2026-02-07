#include "scene.hpp"

#include <sokol_gl.h>
#include <sokol_time.h>

#include <dr/math.hpp>
#include <dr/random.hpp>
#include <dr/span.hpp>

#include <dr/app/debug_draw.hpp>
#include <dr/app/event_handlers.hpp>
#include <dr/app/orbit_camera.hpp>
#include <dr/app/shim/imgui.hpp>
#include <dr/app/task_queue.hpp>
#include <dr/app/thread_pool.hpp>

#include "assets.hpp"
#include "renderer.hpp"
#include "tasks.hpp"
#include "utils.hpp"

namespace dr
{
namespace
{

struct
{
    char const* name = "Geodesic Heat";
    char const* author = "David Reeves";
    struct
    {
        u16 major{0};
        u16 minor{7};
        u16 patch{0};
    } version;
} constexpr scene_info;

struct
{
    Renderer renderer;
    OrbitCamera camera;

    struct
    {
        MeshAsset const* asset{};
        DynamicArray<i32> src_verts;
        Conformal3<f32> xform;
        struct
        {
            Buffer<sizeof(i32)> index;
            Buffer<sizeof(f32[6])> vertex;
            Buffer<sizeof(f32)> func;
        } gpu;
    } mesh;

    Random<> random{1};
    u64 animate_time{};

    TaskQueue task_queue;
    struct
    {
        LoadMeshAsset load_mesh_asset;
        SolveDistance solve_distance;
    } tasks;

    struct
    {
        AssetHandle::Mesh mesh_handle{};
        Param<i32> num_sources{1, 1, 10};
        Param<f32> contour_spacing{0.1f, 0.0f, 1.0f};
        Param<f32> contour_line_width{0.3f, 0.0f, 1.0f};
        Param<f32> contour_speed{0.1f, 0.0f, 1.0f};
        Param<f32> contour_offset{0.0f, 0.0f, 1.0f};
        bool show_color_contour{true};
        bool show_line_contour{true};
        bool animate{true};
    } params;
} state;

void mesh_set_src_verts()
{
    auto& mesh = state.mesh;

    assert(mesh.asset);
    auto random_vert = state.random.generator<i32>(0, mesh.asset->vertices.count());

    auto& src_verts = mesh.src_verts;
    for (isize i = 0; i < size(src_verts); ++i)
        src_verts[i] = random_vert();
}

void mesh_append_src_verts()
{
    auto& mesh = state.mesh;

    assert(mesh.asset);
    auto random_vert = state.random.generator<i32>(0, mesh.asset->vertices.count());

    auto& src_verts = mesh.src_verts;
    while (size(src_verts) < state.params.num_sources.value)
        src_verts.push_back(random_vert());
}

void mesh_set_asset(MeshAsset const* asset)
{
    assert(asset);

    auto& mesh = state.mesh;
    mesh.asset = asset;

    // Initialize source vertices
    mesh.src_verts.resize(state.params.num_sources.value);
    mesh_set_src_verts();

    // Update GPU buffers
    set_mesh_indices(mesh.gpu.index, as_span(asset->faces.vertex_ids));
    set_mesh_vertices(
        mesh.gpu.vertex,
        as_span(asset->vertices.positions),
        as_span(asset->vertices.normals));

    // Fit to unit sphere in world space
    auto const& [cen, rad] = asset->bounds;
    f32 const s = 1.0f / rad;
    mesh.xform = {
        .translation = -cen * s,
        .scale = s,
    };
}

void mesh_set_plot(Span<f32 const> const& values)
{
    assert(values);
    set_mesh_vertices(state.mesh.gpu.func, values);
}

bool mesh_has_plot() { return state.mesh.gpu.func.count > 0; }

void mesh_clear()
{
    auto& mesh = state.mesh;
    mesh.asset = nullptr;
    mesh.src_verts.clear();
    mesh.gpu.index.count = 0;
    mesh.gpu.vertex.count = 0;
    mesh.gpu.func.count = 0;
}

void schedule_task(SolveDistance& task)
{
    using Event = TaskQueue::PollEvent;

    state.task_queue.push(&task, nullptr, [](Event const& event) -> bool {
        auto const task = static_cast<SolveDistance*>(event.task);
        switch (event.type)
        {
            case Event::BeforeSubmit:
            {
                task->input.mesh = state.mesh.asset;
                task->input.source_vertices = //
                    as_span(state.mesh.src_verts).front(state.params.num_sources.value);

                return true;
            };
            case Event::AfterComplete:
            {
                mesh_set_plot(task->output.distance);
                return true;
            };
            default:
            {
                return true;
            };
        }
    });
}

void schedule_task(LoadMeshAsset& task)
{
    using Event = TaskQueue::PollEvent;

    state.task_queue.push(&task, nullptr, [](Event const& event) -> bool {
        auto const task = static_cast<LoadMeshAsset*>(event.task);
        switch (event.type)
        {
            case Event::BeforeSubmit:
            {
                task->input.handle = state.params.mesh_handle;
                return true;
            };
            case Event::AfterComplete:
            {
                mesh_set_asset(task->output.mesh);
                return true;
            };
            default:
            {
                return true;
            };
        }
    });
}

void on_mesh_asset_change()
{
    mesh_clear();
    schedule_task(state.tasks.load_mesh_asset);
    state.task_queue.barrier();
    schedule_task(state.tasks.solve_distance);
}

void draw_settings_tab()
{
    if (ImGui::BeginTabItem("Settings"))
    {
        ImGui::SeparatorText("Model");
        {
            ImGui::BeginDisabled(state.task_queue.size() > 0);

            AssetHandle::Mesh const curr_handle = state.params.mesh_handle;
            if (ImGui::BeginCombo("Shape", get_asset_meta(curr_handle).name))
            {
                for (u8 i = 0; i < AssetHandle::_Mesh_Count; ++i)
                {
                    AssetHandle::Mesh const handle{i};
                    bool const is_curr = (handle == curr_handle);
                    if (ImGui::Selectable(get_asset_meta(handle).name, is_curr))
                    {
                        if (!is_curr)
                        {
                            state.params.mesh_handle = handle;
                            on_mesh_asset_change();
                        }
                    }

                    if (is_curr)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            {
                // NOTE(dr): Changes are only committed back to state on mouse up
                Param<i32>& p = state.params.num_sources;
                static i32 value = p.value;

                ImGui::SliderInt("Source count", &value, p.min, p.max);
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    state.params.num_sources.value = value;
                    mesh_append_src_verts();
                    schedule_task(state.tasks.solve_distance);
                }
            }

            {
                char const* label = (state.params.num_sources.value > 1) //
                    ? "Change sources"
                    : "Change source";

                if (ImGui::Button(label))
                {
                    mesh_set_src_verts();
                    schedule_task(state.tasks.solve_distance);
                }
            }

            ImGui::EndDisabled();
        }
        ImGui::Spacing();

        ImGui::SeparatorText("Display");
        {
            {
                Param<f32>& p = state.params.contour_spacing;
                ImGui::SliderFloat("Contour spacing", &p.value, p.min, p.max, "%.3f");
            }

#if false
            {
                Param<f32>& p = state.params.contour_line_width;
                ImGui::SliderFloat("Contour line width", &p.value, p.min, p.max, "%.3f");
            }
#endif

            if (state.params.animate)
            {
                Param<f32>& p = state.params.contour_speed;
                ImGui::SliderFloat("Contour speed", &p.value, p.min, p.max, "%.3f");
            }
            else
            {
                Param<f32>& p = state.params.contour_offset;
                ImGui::SliderFloat("Contour offset", &p.value, p.min, p.max, "%.3f");
            }

            ImGui::Checkbox("Show color contour", &state.params.show_color_contour);
            ImGui::Checkbox("Show line contour", &state.params.show_line_contour);
            ImGui::Checkbox("Animate", &state.params.animate);
        }
        ImGui::Spacing();

        ImGui::EndTabItem();
    }
}

void draw_about_tab()
{
    if (ImGui::BeginTabItem("About"))
    {
        ImGui::SeparatorText("Info");
        ImGui::TextWrapped("Approximating geodesic distance on surfaces via the heat method");
        ImGui::Spacing();

        ImGui::Text(
            "Version %u.%u.%u",
            scene_info.version.major,
            scene_info.version.minor,
            scene_info.version.patch);
        ImGui::Text("%s", scene_info.author);
        ImGui::TextLinkOpenURL("Source", "https://github.com/davreev/demo-geodesic-heat");
        ImGui::Spacing();

        ImGui::SeparatorText("Controls");
        ImGui::Text("Left click: orbit");
        ImGui::Text("Right click: pan");
        ImGui::Text("Scroll: zoom");
        ImGui::Text("F key: frame shape");
        ImGui::Text("P key: toggle projection");
        ImGui::Spacing();

        ImGui::SeparatorText("References");
        ImGui::TextLinkOpenURL(
            "The Heat Method for Distance Computation",
            "https://www.cs.cmu.edu/~kmcrane/Projects/HeatMethod/index.html");
        ImGui::Spacing();

        ImGui::SeparatorText("Asset Credits");
        {
            auto const& meta = get_asset_meta(AssetHandle::Mesh_Armadillo);
            ImGui::TextLinkOpenURL(meta.name, meta.link_url);
        }
        ImGui::Spacing();

        ImGui::EndTabItem();
    }
}

void draw_main_window()
{
    ImGui::SetNextWindowPos({20.0f, 20.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({200.0f, 0.0f}, {sapp_widthf(), sapp_heightf()});
    constexpr int window_flags = ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::Begin(scene_info.name, nullptr, window_flags);

    if (ImGui::BeginTabBar("TabBar", ImGuiTabBarFlags_None))
    {
        draw_settings_tab();
        draw_about_tab();
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void draw_animated_text(Span<char const*> const messages, f64 const duration, f64 const time)
{
    f64 const t = fract(time / duration);
    ImGui::Text("%s", messages[static_cast<isize>(t * messages.size())]);
}

void draw_status_tooltip()
{
    if (state.task_queue.size() > 0)
    {
        ImGui::BeginTooltip();
        static char const* text[] = {
            "Working",
            "Working.",
            "Working..",
            "Working...",
        };
        draw_animated_text(as_span(text), 3.0, App::time_s());
        ImGui::EndTooltip();
    }
}

void draw_ui()
{
    draw_main_window();
    draw_status_tooltip();
}

void debug_draw_source_normals(Mat4<f32> const& world_to_view)
{
    auto const& mesh = state.mesh;
    assert(mesh.asset);

    Mat4<f32> const local_to_world = mesh.xform.to_matrix();
    Mat4<f32> const local_to_view = world_to_view * local_to_world;

    sgl_matrix_mode_modelview();
    sgl_load_matrix(local_to_view.data());

    sgl_begin_lines();
    sgl_c3f(1.0f, 1.0f, 1.0f);

    auto const& verts = mesh.asset->vertices;
    f32 const scale = mesh.asset->bounds.radius * 0.2f;

    i32 const num_sources = state.params.num_sources.value;
    for (i32 i = 0; i < num_sources; ++i)
    {
        auto const v = mesh.src_verts[i];
        auto const p0 = verts.positions.col(v);
        auto const p1 = (p0 - verts.normals.col(v) * scale).eval();

        sgl_v3f(p0.x(), p0.y(), p0.z());
        sgl_v3f(p1.x(), p1.y(), p1.z());
    }

    sgl_end();
}

void draw_debug(Mat4<f32> const& world_to_view, Mat4<f32> const& view_to_clip)
{
    sgl_defaults();

    sgl_matrix_mode_projection();
    sgl_load_matrix(view_to_clip.data());

    debug_draw_axes(world_to_view, 0.1f);

    if (mesh_has_plot())
        debug_draw_source_normals(world_to_view);

    sgl_draw();
}

void open(void* /*context*/)
{
    ThreadPool::start(1);
    Renderer::init_default_resources();

    // Center camera on unit sphere
    {
        auto& cam = state.camera;
        cam.target.position = vec<3>(0.0f);
        cam.target.radius = 1.2f;
        cam.frame_target_now();

        // Set default orbit
        cam.controls.orbit = {.polar{pi<f32> * 0.3f}, .azimuth{pi<f32> * 0.1f}};
    }

    // Load default mesh asset and solve
    on_mesh_asset_change();
}

void close(void* /*context*/)
{
    release_all_assets();
    ThreadPool::stop();
}

void update(void* /*context*/)
{
    state.camera.update(App::delta_time_s());

    if (state.params.animate)
        state.animate_time += App::delta_time();

    state.task_queue.poll();
}

void draw(void* /*context*/)
{
    auto const& cam = state.camera;
    Mat4<f32> const world_to_view = cam.make_world_to_view();
    Mat4<f32> const view_to_clip = cam.make_view_to_clip(App::aspect());

    auto const& params = state.params;
    f32 const offset = params.contour_offset.value;
    f32 const speed = params.contour_speed.value;
    f32 const time = stm_sec(state.animate_time);
    f32 const offset_now = offset + time * speed;

    ContourColorMaterial const contour_color_mat{
        .spacing = params.contour_spacing.value,
        .offset = offset_now,
    };

    ContourLineMaterial const contour_line_mat{
        .spacing = params.contour_spacing.value,
        .offset = offset_now,
        .line_width = params.contour_line_width.value,
    };

    auto const& mesh = state.mesh;

    MeshPlotGeometry const mesh_plot_geom{
        .index = mesh.gpu.index.buffer,
        .vertex = mesh.gpu.vertex.buffer,
        .func = mesh.gpu.func.buffer,
        .index_count = mesh.gpu.index.count,
        .vertex_count = mesh.gpu.vertex.count,
    };

    MeshPlot const mesh_plot{
        .geometry = &mesh_plot_geom,
        .materials{
            .contour_color = params.show_color_contour ? &contour_color_mat : nullptr,
            .contour_line = params.show_line_contour ? &contour_line_mat : nullptr,
        },
        .transform = mesh.xform,
    };

    state.renderer.render(
        SceneDesc{
            .mesh_plots = {&mesh_plot, mesh_has_plot() ? 1 : 0},
            .camera{
                .world_to_view = world_to_view,
                .view_to_clip = view_to_clip,
            },
        });

    draw_debug(world_to_view, view_to_clip);
    draw_ui();
}

void handle_event(void* /*context*/, App::Event const& event)
{
    camera_handle_mouse_event(event, state.camera);
    camera_handle_touch_event(event, state.camera);

    constexpr auto toggle_projection = []() {
        auto& cam = state.camera;
        if (cam.projection == OrbitCamera::Projection_Perspective)
            cam.projection = OrbitCamera::Projection_Orthographic;
        else
            cam.projection = OrbitCamera::Projection_Perspective;
    };

    switch (event.type)
    {
        case SAPP_EVENTTYPE_KEY_DOWN:
        {
            switch (event.key_code)
            {
                case SAPP_KEYCODE_F:
                {
                    if (is_mouse_over(event))
                        state.camera.frame_target();

                    break;
                };
                case SAPP_KEYCODE_P:
                {
                    if (is_mouse_over(event))
                        toggle_projection();

                    break;
                };
                case SAPP_KEYCODE_R:
                {
                    if (is_mouse_over(event))
                        Renderer::reload_default_shaders();

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

} // namespace

App::Scene scene() { return {scene_info.name, open, close, update, draw, handle_event, nullptr}; }

} // namespace dr