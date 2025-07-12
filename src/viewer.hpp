#pragma once

#include <dr/basic_types.hpp>
#include <dr/math_types.hpp>

#include <dr/app/app.hpp>
#include <dr/app/camera.hpp>
#include <dr/app/gfx_resource.hpp>

namespace dr
{

struct Viewer
{
    struct ContourColorMaterial
    {
        GfxPipeline::Handle pipeline;
        struct
        {
            GfxImage::Handle image;
            GfxSampler::Handle sampler;
        } matcap;
        f32 spacing;
        f32 offset;
    };

    struct ContourLineMaterial
    {
        GfxPipeline::Handle pipeline;
        f32 spacing;
        f32 line_width;
        f32 offset;
    };

    template <isize stride_>
    struct Buffer
    {
        static constexpr isize stride = stride_;

        GfxBuffer buffer;
        isize capacity;
        isize count;

        isize size() const { return count * stride; }
    };

    struct MeshGeometry
    {
        Buffer<sizeof(f32[6])> vertices;
        Buffer<sizeof(i32)> indices;

        void set_vertices(
            Span<Vec3<f32> const> const& positions,
            Span<Vec3<f32> const> const& normals);

        void set_indices(Span<Vec3<i32> const> const& faces);
    };

    struct MeshPlotGeometry
    {
        Buffer<sizeof(f32)> scalars;
        MeshGeometry const* mesh;

        void set_scalars(Span<f32 const> const& values);
    };

    struct MeshPlot
    {
        using Geometry = MeshPlotGeometry;

        Conformal3<f32> transform;
        Geometry const* geometry;
        struct
        {
            ContourColorMaterial const* contour_color;
            ContourLineMaterial const* contour_line;
        } materials;
    };

    struct View
    {
        Camera camera;

        struct
        {
            f32 fov_y{deg_to_rad(60.0f)};
            f32 clip_near{0.01f};
            f32 clip_far{1000.0f};
        } frustum;

        struct
        {
            EasedOrbit orbit{{pi<f32> * -0.25f, pi<f32> * 0.25f}};
            EasedZoom zoom{{1.0f, 1.0f, 0.01, 1000.0}};
            EasedPan pan{};
            f32 sensitivity{5.0f};
        } controls;

        struct
        {
            Vec3<f32> position{};
            f32 radius{1.0f};
        } target;

        struct
        {
            Mat4<f32> view_to_clip;
            Mat4<f32> world_to_view;
            Mat4<f32> world_to_clip;
        } transforms;

        View();
        void update();
        void frame_target();
    };

    struct Input
    {
        Vec2<f32> last_touch_points[2];
        i8 last_num_touches;
        bool mouse_down[3];
    };

    View view;
    Input input;

    static void init_default_resources();
    static void reload_default_shaders();

    template <typename Material>
    static GfxPipeline make_material_pipeline(GfxShader::Handle shader);

    void update();

    template <typename Material, typename Object>
    void draw(Span<Object const> const& objects) const;

    void handle_event(App::Event const& event);
};

} // namespace dr