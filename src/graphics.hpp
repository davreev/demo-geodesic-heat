#pragma once

#include <dr/basic_types.hpp>
#include <dr/math_types.hpp>
#include <dr/span.hpp>

#include <dr/app/gfx_resource.hpp>

namespace dr
{

void init_graphics();

void reload_shaders();

////////////////////////////////////////////////////////////////////////////////
// Geometry

struct RenderMesh
{
    GfxBuffer vertices[2];
    isize vertex_capacity{};
    isize vertex_count{};

    GfxBuffer indices{};
    isize index_capacity{};
    isize index_count{};

    void set_vertices(Span<Vec3<f32> const> const& positions, Span<Vec3<f32> const> const& normals);
    void set_vertices(Span<f32 const> const& scalars);
    void set_indices(Span<Vec3<i32> const> const& faces);

    void bind_resources(sg_bindings& dst) const;
    void dispatch_draw() const { sg_draw(0, index_count, 1); };

  private:
    void set_vertex_capacity(isize value);
    void set_index_capacity(isize value);
};

////////////////////////////////////////////////////////////////////////////////
// Materials

struct ContourColor
{
    struct
    {
        struct
        {
            f32 local_to_clip[16];
            f32 local_to_view[16];
        } vertex;

        struct
        {
            f32 spacing;
            f32 offset;
        } fragment;
    } uniforms{};

    static GfxPipeline::Handle pipeline();
    void bind_resources(sg_bindings& dst) const;
    void apply_uniforms() const;
};

struct ContourLine
{
    struct
    {
        struct
        {
            f32 local_to_clip[16];
            f32 local_to_view[16];
        } vertex;

        struct
        {
            f32 spacing;
            f32 width;
            f32 offset;
        } fragment;
    } uniforms{};

    static GfxPipeline::Handle pipeline();
    void bind_resources(sg_bindings& dst) const;
    void apply_uniforms() const;
};

} // namespace dr