#include "utils.hpp"

namespace dr
{

void set_mesh_indices(Buffer<sizeof(i32)>& dst, Span<Vec3<i32> const> const& faces)
{
    dst.count = faces.size() * 3;
    if (dst.count > dst.capacity)
    {
        init_gfx_resource(
            dst.buffer,
            {
                .size = usize(dst.size()),
                .usage = {.index_buffer = true, .dynamic_update = true},
            });
        dst.capacity = dst.count;
    }

    sg_update_buffer(dst.buffer, to_range(faces));
}

void set_mesh_vertices(
    Buffer<sizeof(f32[6])>& dst,
    Span<Vec3<f32> const> const& positions,
    Span<Vec3<f32> const> const& normals)
{
    assert(positions.size() == normals.size());

    dst.count = positions.size();
    if (dst.count > dst.capacity)
    {
        init_gfx_resource(
            dst.buffer,
            {
                .size = usize(dst.size()),
                .usage = {.vertex_buffer = true, .dynamic_update = true},
            });
        dst.capacity = dst.count;
    }

    sg_append_buffer(dst.buffer, to_range(positions));
    sg_append_buffer(dst.buffer, to_range(normals));
}

void set_mesh_vertices(Buffer<sizeof(f32)>& dst, Span<f32 const> const& values)
{
    dst.count = values.size();
    if (dst.count > dst.capacity)
    {
        init_gfx_resource(
            dst.buffer,
            {
                .size = usize(dst.size()),
                .usage = {.vertex_buffer = true, .dynamic_update = true},
            });
        dst.capacity = dst.count;
    }

    sg_update_buffer(dst.buffer, to_range(values));
}

} // namespace dr