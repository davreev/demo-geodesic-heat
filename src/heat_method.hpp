#pragma once

/*
    Implementation of the heat method for computing geodesic distance on triangle
    meshes

    Refs
    https://www.cs.cmu.edu/~kmcrane/Projects/HeatMethod/paperCACM.pdf
*/

#include <cassert>

#include <Eigen/SparseCholesky>

#include <dr/dynamic_array.hpp>
#include <dr/geometry.hpp>
#include <dr/linalg_reshape.hpp>
#include <dr/math_types.hpp>
#include <dr/mesh_attributes.hpp>
#include <dr/mesh_operators.hpp>
#include <dr/span.hpp>
#include <dr/sparse_linalg_types.hpp>

namespace dr
{

template <typename Real, typename Index>
struct HeatMethod
{
    using Solver = Eigen::SimplicialLDLT<SparseMat<Real>>;

    bool init(
        Span<Vec3<Real> const> const& vertex_positions,
        Span<Vec3<Index> const> const& face_vertices,
        Real const time)
    {
        domain_ = {vertex_positions, face_vertices};

        isize const n_v = vertex_positions.size();
        mass_.resize(n_v);
        u0_.resize(n_v);
        ut_.resize(n_v);
        lap_dist_.resize(n_v);

        // Create cotan stiffness matrix
        make_cotan_laplacian(vertex_positions, face_vertices, coeffs_);
        S_.resize(n_v, n_v);
        S_.setFromTriplets(coeffs_.begin(), coeffs_.end());

        // Create diagonal mass matrix
        vertex_areas_barycentric(vertex_positions, face_vertices, as_span(mass_));

        // Initialize solvers
        if (decomp_heat(time) && decomp_distance())
        {
            status_ = Status_Initialized;
            return true;
        }
        else
        {
            status_ = Status_Default;
            return false;
        }
    }

    bool reinit(Real const time)
    {
        assert(is_init());

        if (!decomp_heat(time))
        {
            status_ = Status_Default;
            return false;
        }

        return true;
    }

    void solve(
        Span<const Index> const& source_vertices,
        Span<Real> const& result,
        bool const store_grads = false)
    {
        assert(is_init());

        auto const& [vert_coords, face_verts] = domain_;
        auto u0 = as_span(u0_);
        auto ut = as_span(ut_);
        auto lap_dist = as_span(lap_dist_);

        // Set initial temperatures
        as_vec(u0).setZero();
        for (auto const v : source_vertices)
            u0[v] = mass_[v];

        // Solve for temperature at the given time
        as_vec(ut) = heat_solver_.solve(as_vec(u0));

        // NOTE(dr): Distance and temperature gradients can either be cached or evaluated on the fly
        // if not needed elsewhere
        if (store_grads)
        {
            // Evaluate tempterature gradient
            grad_ut_.resize(face_verts.size());
            eval_gradient(vert_coords, ut.as_const(), face_verts, as_span(grad_ut_));

            // Reverse and normalize to get approx distance gradient
            grad_dist_.resize(face_verts.size());
            for (isize f = 0; f < face_verts.size(); ++f)
            {
                Covec3<f32> const& g = grad_ut_[f];
                grad_dist_[f] = -g / g.norm();
            }

            // Evaluate divergence of distance gradient
            eval_divergence(
                vert_coords,
                face_verts,
                as<Vec3<Real> const>(as_span(grad_dist_)),
                lap_dist);
        }
        else
        {
            as_vec(lap_dist).setZero();

            // Evaluate the divergence of the normalized temperature gradient
            for (isize f = 0; f < face_verts.size(); ++f)
            {
                auto const& f_v = face_verts[f];

                Covec3<Real> const f_grad_ut = eval_gradient(
                    vert_coords[f_v[0]],
                    vert_coords[f_v[1]],
                    vert_coords[f_v[2]],
                    ut_[f_v[0]],
                    ut_[f_v[1]],
                    ut_[f_v[2]]);

                Covec3<Real> const f_grad_dist = f_grad_ut / -f_grad_ut.norm();
                auto const f_lap_dist = eval_divergence(
                    vert_coords[f_v[0]],
                    vert_coords[f_v[1]],
                    vert_coords[f_v[2]],
                    f_grad_dist.transpose().eval());

                lap_dist[f_v[0]] += f_lap_dist[0];
                lap_dist[f_v[1]] += f_lap_dist[1];
                lap_dist[f_v[2]] += f_lap_dist[2];
            }
        }

        // Solve for geodesic distance
        auto dist = as_vec(result);
        dist = dist_solver_.solve(as_vec(lap_dist));

        // Subtract off mean distance at sources
        {
            f32 sum{0.0};
            for (auto const v : source_vertices)
                sum += dist[v];

            dist.array() -= sum / source_vertices.size();
        }

        status_ = Status_Solved;
    }

    bool is_init() const { return status_ != Status_Default; }

    bool is_solved() const { return status_ == Status_Solved; }

    Span<Real const> temperature() const
    {
        assert(is_solved());
        return as_span(ut_);
    }

    Span<Covec3<Real> const> grad_temperature() const
    {
        assert(is_solved());
        return as_span(grad_ut_);
    }

    Span<Covec3<Real> const> grad_distance() const
    {
        assert(is_solved());
        return as_span(grad_dist_);
    }

    Span<Real const> lap_distance() const
    {
        assert(is_solved());
        return as_span(lap_dist_);
    }

    Solver const& heat_solver() const { return heat_solver_; }

    Solver const& distance_solver() const { return dist_solver_; }

  private:
    enum Status : u8
    {
        Status_Default = 0,
        Status_Initialized,
        Status_Solved,
    };

    struct
    {
        Span<Vec3<Real> const> vertex_positions;
        Span<Vec3<Index> const> face_vertices;
    } domain_{};
    Solver heat_solver_{};
    Solver dist_solver_{};
    SparseMat<Real, Index> S_{};
    SparseMat<Real, Index> A_{};
    DynamicArray<Triplet<Real, Index>> coeffs_{};
    DynamicArray<Real> mass_{};
    DynamicArray<Real> u0_{};
    DynamicArray<Real> ut_{};
    DynamicArray<Covec3<f32>> grad_ut_{};
    DynamicArray<Covec3<f32>> grad_dist_{};
    DynamicArray<Real> lap_dist_{};
    Status status_{};

    bool decomp_heat(Real const time)
    {
        // A = (M - t S)
        A_ = -time * S_;
        A_.diagonal() += as_vec(as_span(mass_));

        heat_solver_.compute(A_);
        return heat_solver_.info() == Eigen::Success;
    }

    bool decomp_distance()
    {
        dist_solver_.compute(S_);
        return dist_solver_.info() == Eigen::Success;
    }
};

} // namespace dr