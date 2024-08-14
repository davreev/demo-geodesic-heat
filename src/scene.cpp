#include "scene.hpp"

#include <cmath>

#include <sokol_gl.h>
#include <sokol_time.h>

#include <dr/math.hpp>
#include <dr/random.hpp>
#include <dr/span.hpp>

#include <dr/app/camera.hpp>
#include <dr/app/debug_draw.hpp>
#include <dr/app/event_handlers.hpp>
#include <dr/app/gfx_utils.hpp>
#include <dr/app/shim/imgui.hpp>
#include <dr/app/task_queue.hpp>
#include <dr/app/thread_pool.hpp>

#include "assets.hpp"
#include "graphics.hpp"
#include "tasks.hpp"

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

enum DisplayMode : u8
{
    DisplayMode_ContourColor = 0,
    DisplayMode_ContourLine,
    _DisplayMode_Count,
};

// clang-format off
struct {
    char const* name = "Geodesic Heat";
    char const* author = "David Reeves";
    struct {
        u16 major{0};
        u16 minor{4};
        u16 patch{0};
    } version;
} constexpr scene_info{};

struct {
    struct {
        RenderMesh mesh;
        struct {
            ContourColor contour_color;
            ContourLine contour_line;
        } materials;
    } gfx;

    MeshAsset const* mesh;
    DynamicArray<i32> source_vertices;
    Random<i32> random_vertex;
    u64 animate_time;

    TaskQueue task_queue;
    struct {
        LoadMeshAsset load_mesh_asset;
        SolveDistance solve_distance;
    } tasks;

    struct {
        f32 fov_y{deg_to_rad(60.0f)};
        f32 clip_near{0.01f};
        f32 clip_far{100.0f};
    } view;

    EasedOrbit orbit{{pi<f32> * -0.25f, pi<f32> * 0.25f}};
    EasedZoom zoom{{1.0f, 1.0f, view.clip_near, view.clip_far}};
    EasedPan pan{};
    Camera camera{make_camera(orbit.current, zoom.current)};

    struct {
        Vec2<f32> last_touch_points[2];
        i8 last_num_touches;
        bool mouse_down[3];
    } input;
    
    struct {
        AssetHandle::Mesh mesh_handle{};
        DisplayMode display_mode{DisplayMode_ContourLine};
        Param<i32> num_sources{1, 1, 10};
        Param<f32> solve_time{0.002f, 0.001f, 0.01f};
        Param<f32> contour_spacing{0.1f, 0.0f, 1.0f};
        Param<f32> contour_width{0.3f, 0.0f, 1.0f};
        Param<f32> contour_speed{0.1f, 0.0f, 1.0f};
        Param<f32> contour_offset{0.0f, 0.0f, 1.0f};
        bool animate{true};
    } params;
} state{};
// clang-format on

void center_camera(Vec3<f32> const& point, f32 const radius)
{
    constexpr f32 pad_scale{1.2f};
    state.camera.pivot.position = point;
    state.zoom.target.distance = radius * pad_scale / std::asin(state.view.fov_y * 0.5);
    state.pan.target.offset = {};
}

void append_source_vertices()
{
    auto& src_verts = state.source_vertices;
    while (size(src_verts) < state.params.num_sources.value)
        src_verts.push_back(state.random_vertex());
}

void reset_source_vertices()
{
    auto& src_verts = state.source_vertices;
    for (isize i = 0; i < size(src_verts); ++i)
        src_verts[i] = state.random_vertex();
}

void set_mesh(MeshAsset const* mesh)
{
    state.mesh = mesh;

    // Center view on new mesh
    center_camera(mesh->bounds.center, mesh->bounds.radius);

    // Initialize source vertices
    {
        state.source_vertices.resize(state.params.num_sources.value);
        state.random_vertex = Random<i32>(0, static_cast<i32>(mesh->vertices.count() - 1), 1);
        reset_source_vertices();
    }

    // Update the render mesh
    {
        auto& render_mesh = state.gfx.mesh;
        render_mesh.set_indices(as_span(mesh->faces.vertex_ids));
        render_mesh.set_vertices(
            as_span(mesh->vertices.positions),
            as_span(mesh->vertices.normals));

        // Set default function using asset tex coords
        render_mesh.set_vertices({mesh->vertices.tex_coords.data(), mesh->vertices.count()});
    }
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
                state.gfx.mesh.set_vertices(task->output.distance);
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

            static char const* const mesh_names[] = {
                "Torus",
                "Double torus",
                "Triple torus",
                "Chen-Gackstatter",
                "Node cluster",
                "Armadillo",
            };

            AssetHandle::Mesh const handle = state.params.mesh_handle;
            if (ImGui::BeginCombo("Shape", mesh_names[handle]))
            {
                for (u8 i = 0; i < AssetHandle::_Mesh_Count; ++i)
                {
                    bool const is_selected = (i == handle);
                    if (ImGui::Selectable(mesh_names[i], is_selected))
                    {
                        if (!is_selected)
                        {
                            state.params.mesh_handle = AssetHandle::Mesh{i};
                            schedule_task(state.tasks.load_mesh_asset);
                            state.task_queue.barrier();
                            schedule_task(state.tasks.solve_distance);
                        }
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            {
                // NOTE(dr): Changes are only committed to global state on mouse up
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
            static char const* mode_names[_DisplayMode_Count]{
                "Color contour",
                "Line contour",
            };

            DisplayMode const mode = state.params.display_mode;
            if (ImGui::BeginCombo("Mode", mode_names[mode]))
            {
                for (u8 i = 0; i < _DisplayMode_Count; ++i)
                {
                    bool const is_selected = (i == mode);

                    if (ImGui::MenuItem(mode_names[i], nullptr, is_selected))
                    {
                        if (!is_selected)
                            state.params.display_mode = DisplayMode{i};
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            {
                Param<f32>& p = state.params.contour_spacing;
                ImGui::SliderFloat("Contour spacing", &p.value, p.min, p.max, "%.3f");
            }

#if false
            if(mode == DisplayMode_ContourLine)
            {
                Param<f32>& p = state.params.contour_width;
                ImGui::SliderFloat("Contour width", &p.value, p.min, p.max, "%.3f");
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
        ImGui::TextWrapped("Approximating geodesic distances on surfaces via the heat method");
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
        ImGui::TextLinkOpenURL("Armadillo", "http://graphics.stanford.edu/data/3Dscanrep/");
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

void debug_draw_source_normals(Mat4<f32> const& local_to_view, Mat4<f32> const& view_to_clip)
{
    sgl_defaults();

    sgl_matrix_mode_modelview();
    sgl_load_matrix(local_to_view.data());

    sgl_matrix_mode_projection();
    sgl_load_matrix(view_to_clip.data());

    {
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
}

void draw_db(Mat4<f32> const& local_to_view, Mat4<f32> const& view_to_clip)
{
    debug_draw_axes(local_to_view, view_to_clip, 0.1f);

    if (state.mesh)
        debug_draw_source_normals(local_to_view, view_to_clip);

    sgl_draw();
}

void open(void* /*context*/)
{
    thread_pool_start(1);
    init_materials();

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
    f32 const t = saturate(5.0 * App::delta_time_s());

    state.orbit.update(t);
    state.orbit.apply(state.camera);

    state.zoom.update(t);
    state.zoom.apply(state.camera);

    state.pan.update(t);
    state.pan.apply(state.camera);

    if (state.params.animate)
        state.animate_time += App::delta_time();

    state.task_queue.poll();
}

void draw(void* /*context*/)
{
    Mat4<f32> const local_to_world = state.mesh //
        ? make_translate(state.mesh->bounds.center)
        : Mat4<f32>::Identity();

    Mat4<f32> const world_to_view = state.camera.transform().inverse_to_matrix();
    Mat4<f32> const local_to_view = world_to_view * local_to_world;
    Mat4<f32> const view_to_clip = make_perspective(
        state.view.fov_y,
        App::aspect(),
        state.view.clip_near,
        state.view.clip_far);

    if (state.mesh)
    {
        RenderPass pass{};

        auto const curr_offset = []() -> f32 {
            f32 const offset = state.params.contour_offset.value;
            f32 const speed = state.params.contour_speed.value;
            f32 const time = stm_sec(state.animate_time);
            return offset + time * speed;
        };

        switch (state.params.display_mode)
        {
            case DisplayMode_ContourColor:
            {
                auto& mat = state.gfx.materials.contour_color;
                {
                    // Update params
                    as_mat<4, 4>(mat.params.vertex.local_to_clip) = view_to_clip * local_to_view;
                    as_mat<4, 4>(mat.params.vertex.local_to_view) = local_to_view;
                    mat.params.fragment.spacing = state.params.contour_spacing.value;
                    mat.params.fragment.offset = curr_offset();
                    mat.params.fragment.time = stm_sec(state.animate_time);
                }
                pass.set_material(mat);
                break;
            }
            case DisplayMode_ContourLine:
            {
                auto& mat = state.gfx.materials.contour_line;
                {
                    // Update params
                    as_mat<4, 4>(mat.params.vertex.local_to_clip) = view_to_clip * local_to_view;
                    as_mat<4, 4>(mat.params.vertex.local_to_view) = local_to_view;
                    mat.params.fragment.spacing = state.params.contour_spacing.value;
                    mat.params.fragment.width = state.params.contour_width.value;
                    mat.params.fragment.offset = curr_offset();
                }
                pass.set_material(mat);
                break;
            }
            default:
            {
            }
        }

        pass.draw_geometry(state.gfx.mesh);
    }

    draw_ui();
    draw_db(local_to_view, view_to_clip);
}

void handle_event(void* /*context*/, App::Event const& event)
{
    camera_handle_mouse_event(
        event,
        state.camera.offset.z(),
        screen_to_view(state.view.fov_y, sapp_heightf()),
        &state.orbit.target,
        &state.zoom.target,
        &state.pan.target,
        state.input.mouse_down);

    switch (event.type)
    {
        case SAPP_EVENTTYPE_KEY_DOWN:
        {
            switch (event.key_code)
            {
                case SAPP_KEYCODE_F:
                {
                    if (is_mouse_over(event))
                    {
                        if (state.mesh)
                        {
                            auto const& [center, radius] = state.mesh->bounds;
                            center_camera(center, radius);
                        }
                        else
                        {
                            center_camera({}, 1.0f);
                        }
                    }
                    break;
                }
                case SAPP_KEYCODE_R:
                {
                    reload_shaders();
                    break;
                }
                default:
                {
                }
            }

            break;
        }
        default:
        {
        }
    }
}

} // namespace

App::Scene scene() { return {scene_info.name, open, close, update, draw, handle_event, nullptr}; }

} // namespace dr