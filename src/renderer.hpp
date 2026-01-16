#pragma once

#include <dr/basic_types.hpp>
#include <dr/dynamic_array.hpp>
#include <dr/math_types.hpp>
#include <dr/span.hpp>
#include <dr/transform.hpp>

#include <dr/app/app.hpp>
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
        // ...
        // ...
    };

    struct DrawCommand
    {
        GfxBindings bindings{};
        GfxPipeline::Handle pipeline{};
        void const* material{};
        void const* geometry{};
        Span<u8 const> material_uniform_data;
        Span<u8 const> geometry_uniform_data;
        struct
        {
            i32 start{};
            i32 size{};
        } object_uniform_slice;
        i32 base_element{};
        i32 num_elements{};
        i32 num_instances{};
    };

    struct PassContext
    {
        DynamicArray<DrawCommand> draw_cmds;
        DynamicArray<u8> uniform_data;
        struct
        {
            i32 start{};
            i32 size{};
        } pass_uniform_slice;
    };

    /// Specialize for different scene types
    template <typename Scene>
    void render(Scene const& scene);

    /// Specialize for different scene object types
    template <Renderer::Pass pass, typename Source>
    static void emit_draw_cmds(Source const& /*src*/, Renderer::PassContext& /*ctx*/)
    {
        // No draw commands emitted by default
    }

  private:
    PassContext pass_;

    /// Orders and submits cached draw commands
    static void submit_draw_cmds(Renderer::PassContext& ctx);
};

/*
    Renderer specializations
*/

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
    // ...
    // ...

    struct
    {
        Mat4<f32> world_to_view;
        Mat4<f32> view_to_clip;
    } camera;
};

template <>
void Renderer::render(SceneDesc const& scene);

template <>
void Renderer::emit_draw_cmds<Renderer::Pass::UnlitOpaque>(
    MeshPlot const& src,
    Renderer::PassContext& ctx);

template <>
void Renderer::emit_draw_cmds<Renderer::Pass::UnlitTransparent>(
    MeshPlot const& src,
    Renderer::PassContext& ctx);

void init_default_gfx_resources();

void reload_default_shaders();

} // namespace dr