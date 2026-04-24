#pragma once

#include <dr/basic_types.hpp>
#include <dr/dynamic_array.hpp>
#include <dr/math_types.hpp>
#include <dr/sliced_array.hpp>
#include <dr/span.hpp>
#include <dr/transform.hpp>

#include <dr/app/draw_context.hpp>
#include <dr/app/gfx_resource.hpp>

namespace dr
{

struct ContourColorMaterial
{
    struct
    {
        GfxView::Handle view{};
        GfxSampler::Handle sampler{};
    } matcap;
    f32 spacing{};
    f32 offset{};

    GfxPipeline::Handle pipeline() const;
};

struct ContourLineMaterial
{
    f32 spacing{};
    f32 offset{};
    f32 line_width{};

    GfxPipeline::Handle pipeline() const;
};

struct MeshPlotGeometry
{
    GfxBuffer::Handle index{};
    GfxBuffer::Handle vertex{};
    GfxBuffer::Handle func{};
    isize index_count{};
    isize vertex_count{};
};

struct MeshPlot
{
    MeshPlotGeometry const* geometry{};
    struct
    {
        ContourColorMaterial const* contour_color{};
        ContourLineMaterial const* contour_line{};
    } materials;
    Conformal3<f32> transform;
};

struct SceneView
{
    Span<MeshPlot const> mesh_plots{};
    // ...

    struct
    {
        Mat4<f32> world_to_view;
        Mat4<f32> view_to_clip;
    } camera;
};

struct Renderer
{
    static void init_default_resources();
    static void reload_default_shaders();
    static void render(SceneView const& scene, DrawContext& draw_ctx);
};

/// Specialize for different source/material combinations
template <typename Material, typename Source>
void emit_draw_cmds(Source const& src, DrawContext& draw_ctx);

template <>
void emit_draw_cmds<ContourColorMaterial>(MeshPlot const& src, DrawContext& draw_ctx);

template <>
void emit_draw_cmds<ContourLineMaterial>(MeshPlot const& src, DrawContext& draw_ctx);

} // namespace dr