#include "assets.hpp"

#include <stb_image.h>

#include <dr/linalg_reshape.hpp>
#include <dr/mesh_attributes.hpp>

#include <dr/app/asset_cache.hpp>
#include <dr/app/file_utils.hpp>
#include <dr/string.hpp>

#include "shim/happly.hpp"

namespace dr
{
namespace
{

struct
{
    AssetCache<MeshAsset> meshes;
    AssetCache<ImageAsset> images;
    AssetCache<ShaderAsset> shaders;
} state;

char const* asset_path(AssetHandle::Mesh const handle)
{
    static constexpr char const* paths[]{
        "assets/models/torus.ply",
        "assets/models/double-torus.ply",
        "assets/models/triple-torus.ply",
        "assets/models/chen-gackstatter-surface.ply",
        "assets/models/node-cluster.ply",
        "assets/models/armadillo-remesh.ply",
    };
    static_assert(size(paths) == AssetHandle::_Mesh_Count);
    return paths[handle];
}

char const* asset_path(AssetHandle::Image const handle)
{
    static constexpr char const* paths[]{
        "assets/images/matcap-white-soft.png",
    };
    static_assert(size(paths) == AssetHandle::_Image_Count);
    return paths[handle];
}

char const* asset_path(AssetHandle::Shader const handle)
{
    static constexpr char const* paths[]{
        "assets/shaders/contour_color.vert.glsl",
        "assets/shaders/contour_color.frag.glsl",
        "assets/shaders/contour_line.vert.glsl",
        "assets/shaders/contour_line.frag.glsl",
    };
    static_assert(size(paths) == AssetHandle::_Shader_Count);
    return paths[handle];
}

bool read_mesh_ply(char const* path, MeshAsset& asset)
{
    using namespace happly;

    try
    {
        PLYData ply{path};
        ply.validate();

        // Assign vertex attributes
        {
            auto& ply_verts = ply.getElement("vertex");

            // Positions
            {
                Span<f32 const> const x = get_property_data<f32>(ply_verts, "x");
                Span<f32 const> const y = get_property_data<f32>(ply_verts, "y");
                Span<f32 const> const z = get_property_data<f32>(ply_verts, "z");

                if (!(x.is_valid() && y.is_valid() && z.is_valid()))
                    return false;

                auto& dst = asset.vertices.positions;
                dst.resize(3, ply_verts.count);
                dst.row(0) = as_covec(x);
                dst.row(1) = as_covec(y);
                dst.row(2) = as_covec(z);
            }

            // Texture coords (optional)
            {
                Span<f32 const> const u = get_property_data<f32>(ply_verts, "uv1");
                Span<f32 const> const v = get_property_data<f32>(ply_verts, "uv2");

                auto& dst = asset.vertices.tex_coords;
                dst.resize(2, ply_verts.count);

                if (u.is_valid())
                    dst.row(0) = as_covec(u);
                else
                    dst.row(0).setConstant(0.0f);

                if (v.is_valid())
                    dst.row(1) = as_covec(v);
                else
                    dst.row(1).setConstant(0.0f);
            }
        }

        // Assign face attributes
        {
            auto& ply_faces = ply.getElement("face");

            // Vertex IDs
            {
                // NOTE(dr): We check a few different naming conventions here
                static constexpr char const* prop_names[]{
                    "vertex_indices", // Used by Blender and Houdini
                    "vertex_index", // Used by Rhino
                    // ...
                };

                Property* prop{};
                for (auto name : prop_names)
                {
                    prop = get_property(ply_faces, name);
                    if (prop != nullptr)
                        break;
                }

                if (prop == nullptr)
                    return false;

                Span<i32 const> prop_data = get_list_property_data<i32>(prop);

                if (!prop_data.is_valid())
                    prop_data = as<i32>(get_list_property_data<u32>(prop));

                if (!prop_data.is_valid())
                    return false;

                asset.faces.vertex_ids = as_mat(prop_data, 3);
            }
        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}

void compute_bounds(MeshAsset& asset)
{
    asset.bounds.center = area_centroid(
        as_span(asset.vertices.positions).as_const(),
        as_span(asset.faces.vertex_ids).as_const());

    asset.bounds.radius = bounding_radius(
        as_span(asset.vertices.positions).as_const(),
        asset.bounds.center);
}

void compute_vertex_normals(MeshAsset& asset)
{
    auto& normals = asset.vertices.normals;
    normals.resize(3, asset.vertices.count());

    vertex_normals_area_weighted(
        as_span(asset.vertices.positions).as_const(),
        as_span(asset.faces.vertex_ids).as_const(),
        as_span(normals));
}

bool load_mesh(String const& path, MeshAsset& asset)
{
    if (read_mesh_ply(path.c_str(), asset))
    {
        compute_vertex_normals(asset);
        compute_bounds(asset);
        return true;
    }
    return false;
}

bool load_image(String const& path, ImageAsset& asset)
{
    constexpr int stride = 4;
    int width, height, src_stride;
    stbi_uc* const image_data = stbi_load(path.c_str(), &width, &height, &src_stride, stride);

    if (image_data != nullptr)
    {
        auto const free_image = [](u8* const data) { stbi_image_free(data); };
        asset = {{image_data, free_image}, width, height, stride};
        return true;
    }
    else
    {
        return false;
    }
}

bool load_shader(String const& path, ShaderAsset& asset)
{
    return read_text_file(path.c_str(), asset.src);
}

} // namespace

MeshAsset const* get_asset(AssetHandle::Mesh const handle, bool const force_reload)
{
    return state.meshes.get(asset_path(handle), load_mesh, force_reload);
}

ImageAsset const* get_asset(AssetHandle::Image const handle, bool const force_reload)
{
    return state.images.get(asset_path(handle), load_image, force_reload);
}

ShaderAsset const* get_asset(AssetHandle::Shader const handle, bool const force_reload)
{
    return state.shaders.get(asset_path(handle), load_shader, force_reload);
}

void release_asset(AssetHandle::Mesh const handle) { state.meshes.remove(asset_path(handle)); }

void release_asset(AssetHandle::Image const handle) { state.images.remove(asset_path(handle)); }

void release_asset(AssetHandle::Shader const handle) { state.shaders.remove(asset_path(handle)); }

void release_all_assets()
{
    state.meshes.clear();
    state.images.clear();
    state.shaders.clear();
}

} // namespace dr