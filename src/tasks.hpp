#pragma once

#include <dr/dynamic_array.hpp>
#include <dr/span.hpp>

#include "assets.hpp"
#include "heat_method.hpp"

namespace dr
{

struct LoadMeshAssetTask
{
    struct
    {
        AssetHandle::Mesh handle;
    } input;

    struct
    {
        MeshAsset const* mesh;
    } output;

    void operator()();
};

struct SolveDistanceTask
{
    enum Error : u8
    {
        Error_None = 0,
        Error_SolveFailed,
        _Error_Count,
    };

    struct
    {
        MeshAsset const* mesh;
        Span<const i32> source_vertices;
    } input;

    struct
    {
        Span<f32> distance;
        Error error;
    } output;

    void operator()();

  private:
    HeatMethod<f32, i32> solver_;
    DynamicArray<f32> distance_;
    MeshAsset const* prev_mesh_;
};

} // namespace dr