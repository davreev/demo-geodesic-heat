#pragma once

#include <memory>

#include <dr/basic_types.hpp>
#include <dr/math_types.hpp>
#include <dr/string.hpp>

namespace dr
{

struct AssetHandle
{
    enum Mesh : u8
    {
        Mesh_Torus = 0,
        Mesh_DoubleTorus,
        Mesh_TripleTorus,
        Mesh_ChenGackstatter,
        Mesh_NodeCluster,
        Mesh_Armadillo,
        // Mesh_Custom, // TODO(dr)
        _Mesh_Count,
    };

    enum Image : u8
    {
        Image_Matcap = 0,
        _Image_Count,
    };

    enum Shader : u8
    {
        Shader_ContourColorVert = 0,
        Shader_ContourColorFrag,
        Shader_ContourLineVert,
        Shader_ContourLineFrag,
        _Shader_Count,
    };
};

struct MeshAsset
{
    struct
    {
        VecArray<f32, 3> positions{};
        VecArray<f32, 3> normals{};
        VecArray<f32, 2> tex_coords{};
        isize count() const { return positions.cols(); };
    } vertices;

    struct
    {
        VecArray<i32, 3> vertex_ids{};
        isize count() const { return vertex_ids.cols(); }
    } faces;

    struct
    {
        Vec3<f32> center{Vec3<f32>::Zero()};
        f32 radius{1.0};
    } bounds;
};

struct ImageAsset
{
    using Delete = void(u8*);
    std::unique_ptr<u8, Delete*> data{nullptr, nullptr};
    isize width{};
    isize height{};
    isize stride{};
    isize size() const { return width * height * stride; }
};

struct ShaderAsset
{
    String src{};
};

MeshAsset const* get_asset(AssetHandle::Mesh const handle, bool const force_reload = false);

void release_asset(AssetHandle::Mesh const handle);

ImageAsset const* get_asset(AssetHandle::Image const handle, bool const force_reload = false);

void release_asset(AssetHandle::Image const handle);

ShaderAsset const* get_asset(AssetHandle::Shader const handle, bool const force_reload = false);

void release_asset(AssetHandle::Shader const handle);

void release_all_assets();

} // namespace dr