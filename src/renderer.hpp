#pragma once

#include <dr/basic_types.hpp>
#include <dr/dynamic_array.hpp>
#include <dr/math_types.hpp>
#include <dr/sliced_array.hpp>
#include <dr/span.hpp>
#include <dr/transform.hpp>

#include <dr/app/draw_command.hpp>
#include <dr/app/gfx_resource.hpp>

namespace dr
{

/// Simple forward renderer
struct Renderer
{
    static void init_default_resources();
    static void reload_default_shaders();

    /// Specialize for different scene types
    template <typename Scene>
    void render(Scene const& scene);

  private:
    DynamicArray<DrawCommand> draw_cmds_;
    SlicedArray<u8> uniform_data_;
};

/// Specialize for different source/material combinations
template <typename Material, typename Source>
void emit_draw_cmds(
    Source const& src,
    DynamicArray<DrawCommand>& draw_cmds,
    SlicedArray<u8>& uniform_data);

struct ContourColorMaterial
{
    struct
    {
        GfxImage::Handle image{};
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

struct SceneDesc
{
    Span<MeshPlot const> mesh_plots{};
    // ...

    struct
    {
        Mat4<f32> world_to_view;
        Mat4<f32> view_to_clip;
    } camera;
};

/*
    Specializations
*/

template <>
void Renderer::render(SceneDesc const& scene);

template <>
void emit_draw_cmds<ContourColorMaterial>(
    MeshPlot const& src,
    DynamicArray<DrawCommand>& draw_cmds,
    SlicedArray<u8>& uniform_data);

template <>
void emit_draw_cmds<ContourLineMaterial>(
    MeshPlot const& src,
    DynamicArray<DrawCommand>& draw_cmds,
    SlicedArray<u8>& uniform_data);

} // namespace dr