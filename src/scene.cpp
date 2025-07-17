#include "scene.hpp"

#include <sokol_gl.h>
#include <sokol_time.h>

#include <dr/math.hpp>
#include <dr/random.hpp>
#include <dr/span.hpp>

#include <dr/app/debug_draw.hpp>
#include <dr/app/event_handlers.hpp>
#include <dr/app/shim/imgui.hpp>
#include <dr/app/task_queue.hpp>
#include <dr/app/thread_pool.hpp>

#include "assets.hpp"
#include "tasks.hpp"
#include "viewer.hpp"

namespace dr
{
namespace
{

template <typename Scalar>
struct Param
{
    Scalar value{};
    Scalar min{};
    Scalar max{};
};

// clang-format off
struct {
    char const* name = "Geodesic Heat";
    char const* author = "David Reeves";
    struct {
        u16 major{0};
        u16 minor{7};
        u16 patch{0};
    } version;
} constexpr scene_info{};

struct {
    Viewer viewer;
    struct {
        Viewer::ContourColorMaterial contour_color_material;
        Viewer::ContourLineMaterial contour_line_material;
        Viewer::MeshGeometry mesh_geom;
        Viewer::MeshPlotGeometry mesh_plot_geom;
        Viewer::MeshPlot mesh_plot;
    } scene;

    MeshAsset const* mesh;
    DynamicArray<i32> source_vertices;
    Random<> random{1};
    u64 animate_time;

    TaskQueue task_queue;
    struct {
        LoadMeshAsset load_mesh_asset;
        SolveDistance solve_distance;
    } tasks;

    struct {
        AssetHandle::Mesh mesh_handle;
        Param<i32> num_sources{1, 1, 10};
        Param<f32> solve_time{0.002f, 0.001f, 0.01f};
        Param<f32> contour_spacing{0.1f, 0.0f, 1.0f};
        Param<f32> contour_line_width{0.3f, 0.0f, 1.0f};
        Param<f32> contour_speed{0.1f, 0.0f, 1.0f};
        Param<f32> contour_offset{0.0f, 0.0f, 1.0f};
        bool show_color_contour{true};
        bool show_line_contour{true};
        bool animate{true};
    } params;
} state{};
// clang-format on

void append_source_vertices()
{
    assert(state.mesh);
    auto random_vert = state.random.generator<i32>(0, state.mesh->vertices.count());

    auto& src_verts = state.source_vertices;
    while (size(src_verts) < state.params.num_sources.value)
        src_verts.push_back(random_vert());
}

void reset_source_vertices()
{
    assert(state.mesh);
    auto random_vert = state.random.generator<i32>(0, state.mesh->vertices.count());

    auto& src_verts = state.source_vertices;
    for (isize i = 0; i < size(src_verts); ++i)
        src_verts[i] = random_vert();
}

void set_mesh(MeshAsset const* mesh)
{
    state.mesh = mesh;

    // Initialize source vertices
    {
        state.source_vertices.resize(state.params.num_sources.value);
        reset_source_vertices();
    }

    // Update mesh geometry
    {
        auto& geom = state.scene.mesh_geom;
        geom.set_indices(as_span(mesh->faces.vertex_ids));
        geom.set_vertices(as_span(mesh->vertices.positions), as_span(mesh->vertices.normals));
    }

    // Update mesh plot
    {
        auto& plot = state.scene.mesh_plot;
        plot.geometry = nullptr;

        // Fit to unit sphere in world space
        auto const& [cen, rad] = mesh->bounds;
        f32 const s = 1.0f / rad;
        plot.transform.translation = -cen * s;
        plot.transform.scale = s;
    }
}

void set_plot(Span<f32 const> const& values)
{
    auto& geom = state.scene.mesh_plot_geom;
    geom.set_scalars(values);

    auto& plot = state.scene.mesh_plot;
    plot.geometry = &geom;
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
                task->input.mesh = state.mesh;
                task->input.source_vertices = //
                    as_span(state.source_vertices).front(state.params.num_sources.value);

                return true;
            };
            case Event::AfterComplete:
            {
                set_plot(task->output.distance);
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
                set_mesh(task->output.mesh);
                return true;
            };
            default:
            {
                return true;
            };
        }
    });
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
                            schedule_task(state.tasks.load_mesh_asset);
                            state.task_queue.barrier();
                            schedule_task(state.tasks.solve_distance);
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
                    append_source_vertices();
                    schedule_task(state.tasks.solve_distance);
                }
            }

            {
                char const* label = (state.params.num_sources.value > 1) //
                    ? "Change sources"
                    : "Change source";

                if (ImGui::Button(label))
                {
                    reset_source_vertices();
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
    constexpr int window_flags = ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::Begin(scene_info.name, nullptr, window_flags);
    ImGui::PushItemWidth(200.0f);

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

void debug_draw_source_normals(Mat4<f32> const& local_to_view)
{
    sgl_matrix_mode_modelview();
    sgl_load_matrix(local_to_view.data());

    sgl_begin_lines();
    sgl_c3f(1.0f, 1.0f, 1.0f);

    auto const& verts = state.mesh->vertices;
    i32 const num_sources = state.params.num_sources.value;
    f32 scale = state.mesh->bounds.radius * 0.2f;

    for (i32 i = 0; i < num_sources; ++i)
    {
        auto const v = state.source_vertices[i];
        auto const p0 = verts.positions.col(v);
        auto const p1 = (p0 - verts.normals.col(v) * scale).eval();

        sgl_v3f(p0.x(), p0.y(), p0.z());
        sgl_v3f(p1.x(), p1.y(), p1.z());
    }

    sgl_end();
}

void draw_debug(Viewer::DrawContext const& ctx)
{
    auto const& xforms = ctx.transforms;

    sgl_defaults();

    sgl_matrix_mode_projection();
    sgl_load_matrix(xforms.view_to_clip.data());

    debug_draw_axes(xforms.world_to_view, 0.1f);

    auto const& plot = state.scene.mesh_plot;
    if (plot.geometry)
    {
        Mat4<f32> const local_to_world = plot.transform.to_matrix();
        debug_draw_source_normals(xforms.world_to_view * local_to_world);
    }

    sgl_draw();
}

void open(void* /*context*/)
{
    thread_pool_start(1);

    Viewer::init_default_resources();

    // Initialize scene
    {
        auto& scene = state.scene;
        scene.mesh_plot_geom.mesh = &scene.mesh_geom;

        auto& plot = scene.mesh_plot;
        plot.materials.contour_color = &scene.contour_color_material;
        plot.materials.contour_line = &scene.contour_line_material;
    }

    // Center camera on unit sphere
    {
        auto& view = state.viewer.view;
        view.target.position = vec<3>(0.0f);
        view.target.radius = 1.2f;
        view.frame_target();
    }

    // Load default mesh asset and solve
    {
        schedule_task(state.tasks.load_mesh_asset);
        state.task_queue.barrier();
        schedule_task(state.tasks.solve_distance);
    }
}

void close(void* /*context*/)
{
    release_all_assets();
    thread_pool_stop();
}

void update(void* /*context*/)
{
    state.viewer.update();

    if (state.params.animate)
        state.animate_time += App::delta_time();

    state.task_queue.poll();
}

void draw(void* /*context*/)
{
    // Update material params
    {
        f32 const offset = state.params.contour_offset.value;
        f32 const speed = state.params.contour_speed.value;
        f32 const time = stm_sec(state.animate_time);
        f32 const offset_now = offset + time * speed;

        {
            auto& mat = state.scene.contour_color_material;
            mat.spacing = state.params.contour_spacing.value;
            mat.offset = offset_now;
        }

        {
            auto& mat = state.scene.contour_line_material;
            mat.spacing = state.params.contour_spacing.value;
            mat.line_width = state.params.contour_line_width.value;
            mat.offset = offset_now;
        }
    }

    // Submit draw calls
    {
        auto ctx = state.viewer.make_draw_context();

        if (state.params.show_color_contour)
            ctx.draw<Viewer::ContourColorMaterial>(state.scene.mesh_plot);

        if (state.params.show_line_contour)
            ctx.draw<Viewer::ContourLineMaterial>(state.scene.mesh_plot);

        draw_debug(ctx);
        draw_ui();
    }
}

void handle_event(void* /*context*/, App::Event const& event)
{
    state.viewer.handle_event(event);

    switch (event.type)
    {
        case SAPP_EVENTTYPE_KEY_DOWN:
        {
            switch (event.key_code)
            {
                case SAPP_KEYCODE_F:
                {
                    if (is_mouse_over(event))
                        state.viewer.view.frame_target();

                    break;
                };
                case SAPP_KEYCODE_R:
                {
                    if (is_mouse_over(event))
                        Viewer::reload_default_shaders();

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