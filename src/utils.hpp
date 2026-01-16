#pragma once

#include <dr/basic_types.hpp>
#include <dr/math_types.hpp>
#include <dr/span.hpp>

#include <dr/app/gfx_resource.hpp>

namespace dr
{

template <typename Scalar>
struct Param
{
    Scalar value{};
    Scalar min{};
    Scalar max{};
};

template <isize stride_>
struct Buffer
{
    static constexpr isize stride = stride_;
    GfxBuffer buffer;
    isize capacity{};
    isize count{};
    isize size() const { return count * stride; }
};

void set_mesh_indices(Buffer<sizeof(i32)>& dst, Span<Vec3<i32> const> const& faces);

void set_mesh_vertices(
    Buffer<sizeof(f32[6])>& dst,
    Span<Vec3<f32> const> const& positions,
    Span<Vec3<f32> const> const& normals);

void set_mesh_vertices(Buffer<sizeof(f32)>& dst, Span<f32 const> const& values);

template <typename T>
sg_range to_range(Span<T> const& span)
{
    return {span.data(), span.size() * sizeof(T)};
}

template <typename Resource>
void init_gfx_resource(Resource& res, typename Resource::Desc const& desc)
{
    if (res.is_valid())
        res.init(desc);
    else
        res = Resource::make(desc);
}

// Returns the given handle if it's valid. Otherwise, returns the given default.
template <typename Handle>
Handle const valid_or(Handle const handle, Handle const other)
{
    return (handle.id == SG_INVALID_ID) ? other : handle;
}

} // namespace dr