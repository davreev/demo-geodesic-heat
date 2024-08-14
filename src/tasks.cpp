#include "tasks.hpp"

#include <cassert>

#include <dr/math.hpp>

namespace dr
{
namespace
{

f32 mean_edge_length(
    Span<Vec3<f32> const> const vertex_positions,
    Span<Vec3<i32> const> const face_vertices)
{
    f32 length_sum{0.0f};
    for (auto const& f_v : face_vertices)
    {
        Vec3<f32> const& a = vertex_positions[f_v[0]];
        Vec3<f32> const& b = vertex_positions[f_v[1]];
        Vec3<f32> const& c = vertex_positions[f_v[2]];
        length_sum += (a - b).norm() + (b - c).norm() + (c - a).norm();
    }

    // NOTE(dr): This assumes the mesh has no boundary
    isize const num_edges = face_vertices.size() * 6;
    return length_sum / num_edges;
}

} // namespace

void LoadMeshAsset::operator()()
{
    output.mesh = get_asset(input.handle);
    assert(output.mesh);
}

void SolveDistance::operator()()
{
    assert(input.mesh);

    // Reinitialize solver if input mesh changed
    if (input.mesh != prev_mesh_)
    {
        // NOTE(dr): Paper recommends square mean edge length as a good choice for t
        f32 const mean_edge_len = mean_edge_length(
            as_span(input.mesh->vertices.positions),
            as_span(input.mesh->faces.vertex_ids));

        // NOTE(dr): Solve tends to fail for values less than this
        constexpr f32 min_time = 0.005f;
        f32 const time = max(mean_edge_len * mean_edge_len, min_time);

        // Initialize solver
        bool const ok = solver_.init(
            as_span(input.mesh->vertices.positions),
            as_span(input.mesh->faces.vertex_ids),
            time);

        if (!ok)
        {
            output.distance = {};
            output.error = Error_SolveFailed;
            return;
        }

        prev_mesh_ = input.mesh;
    }

    // Solve distance
    distance_.resize(input.mesh->vertices.count());
    solver_.solve(
        as_span(input.mesh->vertices.positions),
        as_span(input.mesh->faces.vertex_ids),
        input.source_vertices,
        as_span(distance_));

    output.distance = as_span(distance_);
    output.error = {};
}

} // namespace dr