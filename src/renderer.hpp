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

using GfxBindings = sg_bindings;

/// Simple forward renderer
struct Renderer
{
    enum struct Pass : u8
    {
        Undefined = 0,
        UnlitOpaque,
        UnlitTransparent,
        // ...
    };

    static void init_default_resources();
    static void reload_default_shaders();

    /// Specialize for different scene types
    template <typename Scene>
    void render(Scene const& scene);

    /// Specialize for different scene object types
    template <Pass pass, typename Source>
    static void emit_draw_cmds(
        Source const& src,
        DynamicArray<DrawCommand>& draw_cmds,
        SlicedArray<u8>& uniform_data);

  private:
    DynamicArray<DrawCommand> draw_cmds_;
    SlicedArray<u8> uniform_data_;
};

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
    Span<u8 const> uniform_data() const;
};

struct ContourLineMaterial
{
    f32 spacing{};
    f32 line_width{};
    f32 offset{};

    GfxPipeline::Handle pipeline() const;
    Span<u8 const> uniform_data() const;
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
void Renderer::emit_draw_cmds<Renderer::Pass::UnlitOpaque>(
    MeshPlot const& src,
    DynamicArray<DrawCommand>& draw_cmds,
    SlicedArray<u8>& uniform_data);

template <>
void Renderer::emit_draw_cmds<Renderer::Pass::UnlitTransparent>(
    MeshPlot const& src,
    DynamicArray<DrawCommand>& draw_cmds,
    SlicedArray<u8>& uniform_data);

} // namespace dr