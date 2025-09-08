#pragma once

#include <tuple>

#include <dr/basic_types.hpp>
#include <dr/math_types.hpp>
#include <dr/meta.hpp>

#include <dr/app/app.hpp>
#include <dr/app/camera.hpp>
#include <dr/app/gfx_resource.hpp>

namespace dr
{

struct Viewer
{
    struct Material
    {
        GfxPipeline::Handle pipeline;
    };

    struct ContourColorMaterial : Material
    {
        struct
        {
            GfxImage::Handle image;
            GfxSampler::Handle sampler;
        } matcap;
        f32 spacing;
        f32 offset;

        static GfxPipeline make_pipeline(GfxShader::Handle shader);
    };

    struct ContourLineMaterial : Material
    {
        f32 spacing;
        f32 line_width;
        f32 offset;

        static GfxPipeline make_pipeline(GfxShader::Handle shader);
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

    template <typename Geometry_, typename... Materials_>
    struct Object
    {
        using Geometry = Geometry_;
        using Materials = TypePack<Materials_...>;

        template <typename T>
        using ConstPtr = T const*;

        Geometry const* geometry;
        std::tuple<ConstPtr<Materials_>...> materials;
    };

    struct MeshPlot : Object<MeshPlotGeometry, ContourColorMaterial, ContourLineMaterial>
    {
        Conformal3<f32> transform;
    };

    struct DrawContext
    {
        using GfxBindings = sg_bindings;

        struct
        {
            Mat4<f32> view_to_clip;
            Mat4<f32> world_to_view;
            Mat4<f32> world_to_clip;
        } transforms;

        GfxPipeline::Handle pipeline;
        GfxBindings bindings;
        void const* material;
        void const* geometry;

        template <int material_id, typename Object>
        void draw(Object const& object);
    };

    static void init_default_resources();

    static void reload_default_shaders();

    static DrawContext make_draw_context(
        Mat4<f32> const& world_to_view,
        Mat4<f32> const& view_to_clip);
};

} // namespace dr