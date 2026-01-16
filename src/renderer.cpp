#include "renderer.hpp"

#include <algorithm>
#include <type_traits>

#include <dr/linalg_reshape.hpp>
#include <dr/memory.hpp>

#include "assets.hpp"
#include "utils.hpp"

namespace dr
{
namespace
{

enum struct UniformBlock : u8
{
    Pass = 0,
    Material,
    Geometry,
    Object,
};

// NOTE(dr): The assigned shader stage doesn't appear to matter when using OpenGL backends
static sg_shader_stage const shader_stage_any = SG_SHADERSTAGE_VERTEX;

sg_shader_uniform_block unlit_pass_uniform_block()
{
    return {
        .stage = shader_stage_any,
        .size = sizeof(f32[16 * 2]),
        .glsl_uniforms{
            {
                .type = SG_UNIFORMTYPE_FLOAT4,
                .array_count = 4,
                .glsl_name = "pass.world_to_view.data",
            },
            {
                .type = SG_UNIFORMTYPE_FLOAT4,
                .array_count = 4,
                .glsl_name = "pass.world_to_clip.data",
            },
        },
    };
}

template <typename T>
struct Impl;

template <>
struct Impl<ContourColorMaterial>
{
    static inline GfxPipeline default_pipeline;
    static inline GfxShader default_shader;
    struct
    {
        GfxImage image;
        GfxSampler sampler;
    } inline static default_matcap;

    static GfxShader::Desc shader_desc(char const* const vs_src, char const* const fs_src)
    {
        return {
            .vertex_func{.source = vs_src},
            .fragment_func{.source = fs_src},
            .uniform_blocks{
                unlit_pass_uniform_block(),
                {
                    // Material block
                    .stage = shader_stage_any,
                    .size = sizeof(f32[2]),
                    .glsl_uniforms{
                        {
                            .type = SG_UNIFORMTYPE_FLOAT,
                            .glsl_name = "material.spacing",
                        },
                        {
                            .type = SG_UNIFORMTYPE_FLOAT,
                            .glsl_name = "material.offset",
                        },
                    },
                },
                {
                    // Geometry block
                    // ...
                },
                {
                    // Object block
                    .stage = shader_stage_any,
                    .size = sizeof(f32[16 * 1]),
                    .glsl_uniforms{
                        {
                            .type = SG_UNIFORMTYPE_FLOAT4,
                            .array_count = 4,
                            .glsl_name = "object.local_to_world.data",
                        },
                    },
                },
            },
            .images{
                {.stage = shader_stage_any},
            },
            .samplers{
                {.stage = shader_stage_any},
            },
            .image_sampler_pairs{
                {
                    .stage = shader_stage_any,
                    .image_slot = 0,
                    .sampler_slot = 0,
                    .glsl_name = "matcap",
                },
            },
        };
    }

    static GfxPipeline::Desc pipeline_desc(GfxShader::Handle const shader)
    {
        return {
            .shader = shader,
            .layout{
                .attrs{
                    {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT3},
                    {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT3},
                    {.buffer_index = 2, .format = SG_VERTEXFORMAT_FLOAT},
                },
            },
            .depth{
                .compare = SG_COMPAREFUNC_LESS,
                .write_enabled = true,
            },
            .index_type = SG_INDEXTYPE_UINT32,
            .face_winding = SG_FACEWINDING_CCW,
        };
    }

    static GfxImage::Desc matcap_image_desc(
        void const* const data,
        int const width,
        int const height)
    {
        return {
            .width = width,
            .height = height,
            .usage = SG_USAGE_IMMUTABLE,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .data{
                .subimage{
                    {
                        {.ptr = data, .size = usize(width * height * 4)},
                    },
                },
            },
        };
    }

    static GfxSampler::Desc matcap_sampler_desc(void)
    {
        return {
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
        };
    }

    static void init_default_shader()
    {
        ShaderAsset const* vs = get_asset(AssetHandle::Shader_ContourColorVert, true);
        assert(vs);

        ShaderAsset const* fs = get_asset(AssetHandle::Shader_ContourColorFrag, true);
        assert(fs);

        default_shader.init(shader_desc(vs->src.c_str(), fs->src.c_str()));
        assert(default_shader.is_valid());
    };

    static void init_default_resources()
    {
        assert(!default_pipeline.is_valid());

        default_shader = GfxShader::alloc();
        init_default_shader();

        default_pipeline = GfxPipeline::make(pipeline_desc(default_shader));
        assert(default_pipeline.is_valid());

        {
            ImageAsset const* image = get_asset(AssetHandle::Image_Matcap);
            assert(image);

            default_matcap.image = GfxImage::make(
                matcap_image_desc(image->data.get(), image->width, image->height));
            assert(default_matcap.image.is_valid());

            default_matcap.sampler = GfxSampler::make(matcap_sampler_desc());
            assert(default_matcap.sampler.is_valid());
        }
    };
};

template <>
struct Impl<ContourLineMaterial>
{
    inline static GfxPipeline default_pipeline;
    inline static GfxShader default_shader;

    static GfxShader::Desc shader_desc(char const* const vs_src, char const* const fs_src)
    {
        return {
            .vertex_func{.source = vs_src},
            .fragment_func{.source = fs_src},
            .uniform_blocks{
                unlit_pass_uniform_block(),
                {
                    // Material block
                    .stage = shader_stage_any,
                    .size = sizeof(f32[3]),
                    .glsl_uniforms{
                        {
                            .type = SG_UNIFORMTYPE_FLOAT,
                            .glsl_name = "material.spacing",
                        },
                        {
                            .type = SG_UNIFORMTYPE_FLOAT,
                            .glsl_name = "material.line_width",
                        },
                        {
                            .type = SG_UNIFORMTYPE_FLOAT,
                            .glsl_name = "material.offset",
                        },
                    },
                },
                {
                    // Geometry block
                    // ...
                },
                {
                    // Object block
                    .stage = shader_stage_any,
                    .size = sizeof(f32[16 * 1]),
                    .glsl_uniforms{
                        {
                            .type = SG_UNIFORMTYPE_FLOAT4,
                            .array_count = 4,
                            .glsl_name = "object.local_to_world.data",
                        },
                    },
                },
            },
        };
    }

    static GfxPipeline::Desc pipeline_desc(GfxShader::Handle const shader)
    {
        return {
            .shader = shader,
            .layout{
                .attrs{
                    {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT3},
                    {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT3},
                    {.buffer_index = 2, .format = SG_VERTEXFORMAT_FLOAT},
                },
            },
            .depth{
                .compare = SG_COMPAREFUNC_LESS_EQUAL,
                .write_enabled = false,
            },
            .colors{
                {
                    .blend{
                        .enabled = true,
                        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    },
                },
            },
            .index_type = SG_INDEXTYPE_UINT32,
            .face_winding = SG_FACEWINDING_CCW,
        };
    }

    static void init_default_shader()
    {
        ShaderAsset const* vs = get_asset(AssetHandle::Shader_ContourLineVert, true);
        assert(vs);

        ShaderAsset const* fs = get_asset(AssetHandle::Shader_ContourLineFrag, true);
        assert(fs);

        default_shader.init(shader_desc(vs->src.c_str(), fs->src.c_str()));
        assert(default_shader.is_valid());
    };

    static void init_default_resources()
    {
        assert(!default_pipeline.is_valid());

        default_shader = GfxShader::alloc();
        init_default_shader();

        default_pipeline = GfxPipeline::make(pipeline_desc(default_shader));
        assert(default_pipeline.is_valid());
    };
};

template <>
struct Impl<SceneDesc>
{
    // NOTE(dr): Can be specialized for different passes (e.g. lit vs unlit)
    template <Renderer::Pass /*pass*/>
    static void set_pass_uniforms(SceneDesc const& src, Renderer::PassContext& ctx)
    {
        // Append pass uniform data
        i32 const slice_start = ctx.uniform_data.size();
        struct
        {
            f32 world_to_view[16];
            f32 world_to_clip[16];
        } u;
        as_mat<4, 4>(u.world_to_view) = src.camera.world_to_view;
        as_mat<4, 4>(u.world_to_clip) = src.camera.view_to_clip * src.camera.world_to_view;
        append_bytes(ctx.uniform_data, u);

        // Store stable buffer slice
        ctx.pass_uniform_slice = {
            .start = slice_start,
            .size = sizeof(u),
        };
    }

    // NOTE(dr): Can be specialized for different passes (e.g. lit vs unlit)
    template <Renderer::Pass pass>
    static void emit_draw_cmds(SceneDesc const& src, Renderer::PassContext& ctx)
    {
        set_pass_uniforms<pass>(src, ctx);

        for (auto const& obj : src.mesh_plots)
            Renderer::emit_draw_cmds<pass>(obj, ctx);

        // ...
        // ...
        // ...
    }
};

void apply_uniforms(UniformBlock const block, Span<u8 const> const data)
{
    sg_apply_uniforms(int(block), to_range(data));
}

template <typename T>
void append_bytes(DynamicArray<u8>& buf, T const& obj)
{
    static_assert(std::is_trivially_copyable_v<T>);
    constexpr usize n = sizeof(T);
    u8 bytes[n];
    std::memcpy(bytes, &obj, n);
    buf.insert(buf.end(), bytes, bytes + n);
}

} // namespace

template <>
void Renderer::render(SceneDesc const& scene)
{
    using Impl = Impl<SceneDesc>;

    Impl::emit_draw_cmds<Pass::UnlitOpaque>(scene, pass_);
    submit_draw_cmds(pass_);

    Impl::emit_draw_cmds<Pass::UnlitTransparent>(scene, pass_);
    submit_draw_cmds(pass_);
}

GfxPipeline::Handle ContourColorMaterial::pipeline() const
{
    return Impl<ContourColorMaterial>::default_pipeline;
}

Span<u8 const> ContourColorMaterial::uniform_data() const
{
    return {as<u8>(&spacing), sizeof(f32[2])};
}

GfxPipeline::Handle ContourLineMaterial::pipeline() const
{
    return Impl<ContourLineMaterial>::default_pipeline;
}

Span<u8 const> ContourLineMaterial::uniform_data() const
{
    return {as<u8>(&spacing), sizeof(f32[3])};
}

void Renderer::submit_draw_cmds(PassContext& ctx)
{
    // Order draw commands by pipeline, then material, then geometry
    std::sort(
        ctx.draw_cmds.begin(),
        ctx.draw_cmds.end(),
        [](DrawCommand const& a, DrawCommand const& b) {
            if (a.pipeline.id != b.pipeline.id)
                return a.pipeline.id < b.pipeline.id;
            else if (a.material != b.material)
                return a.material < b.material;
            else
                return a.geometry < b.geometry;
        });

    Span<u8 const> const uniform_data = as_span(ctx.uniform_data);
    GfxPipeline::Handle pipeline{};
    void const* geometry = nullptr;
    void const* material = nullptr;
    
    // Submit draw commands
    for (auto const& cmd : ctx.draw_cmds)
    {
        if (cmd.pipeline.id != pipeline.id)
        {
            pipeline = cmd.pipeline;
            sg_apply_pipeline(pipeline);

            // Reapply pass uniforms when pipeline changes
            if (ctx.pass_uniform_slice.size > 0)
            {
                auto const [start, size] = ctx.pass_uniform_slice;
                apply_uniforms(UniformBlock::Pass, uniform_data.segment(start, size));
            }

            geometry = nullptr;
            material = nullptr;
        }

        bool bindings_dirty = false;

        if (cmd.material != material)
        {
            if (cmd.material_uniform_data)
                apply_uniforms(UniformBlock::Material, cmd.material_uniform_data);

            material = cmd.material;
            bindings_dirty = true;
        }

        if (cmd.geometry != geometry)
        {
            if (cmd.geometry_uniform_data)
                apply_uniforms(UniformBlock::Geometry, cmd.geometry_uniform_data);

            geometry = cmd.geometry;
            bindings_dirty = true;
        }

        if (bindings_dirty)
            sg_apply_bindings(cmd.bindings);

        if (cmd.object_uniform_slice.size > 0)
        {
            auto const [start, size] = cmd.object_uniform_slice;
            apply_uniforms(UniformBlock::Object, uniform_data.segment(start, size));
        }

        sg_draw(cmd.base_element, cmd.num_elements, cmd.num_instances);
    }

    // Reset pass context
    ctx.draw_cmds.clear();
    ctx.uniform_data.clear();
    ctx.pass_uniform_slice = {};
}

template <>
void Renderer::emit_draw_cmds<Renderer::Pass::UnlitOpaque>(
    MeshPlot const& src,
    Renderer::PassContext& ctx)
{
    // Skip if material isn't assigned
    auto const mat = src.materials.contour_color;
    if (mat == nullptr)
        return;

    // Append uniform data
    i32 const slice_start = ctx.uniform_data.size();
    struct
    {
        f32 local_to_world[16];
    } u;
    as_mat<4, 4>(u.local_to_world) = src.transform.to_matrix();
    append_bytes(ctx.uniform_data, u);

    // Append draw cmd
    using MatImpl = Impl<ContourColorMaterial>;
    auto const geom = src.geometry;
    ctx.draw_cmds.push_back({
        .bindings{
            .vertex_buffers{
                geom->vertex,
                geom->vertex,
                geom->func,
            },
            .vertex_buffer_offsets{
                0,
                int(geom->vertex_count * sizeof(f32[3])),
                0,
            },
            .index_buffer = geom->index,
            .images{
                valid_or(mat->matcap.image, MatImpl::default_matcap.image.handle()),
            },
            .samplers{
                valid_or(mat->matcap.sampler, MatImpl::default_matcap.sampler.handle()),
            },
        },
        .pipeline = mat->pipeline(),
        .material = mat,
        .geometry = geom,
        .material_uniform_data = mat->uniform_data(),
        .object_uniform_slice{
            .start = slice_start,
            .size = sizeof(u),
        },
        .num_elements = int(geom->index_count),
        .num_instances = 1,
    });
}

template <>
void Renderer::emit_draw_cmds<Renderer::Pass::UnlitTransparent>(
    MeshPlot const& src,
    Renderer::PassContext& ctx)
{
    // Skip if material isn't assigned
    auto const mat = src.materials.contour_line;
    if (mat == nullptr)
        return;

    // Append uniform data
    i32 const slice_start = ctx.uniform_data.size();
    struct
    {
        f32 local_to_world[16];
    } u;
    as_mat<4, 4>(u.local_to_world) = src.transform.to_matrix();
    append_bytes(ctx.uniform_data, u);

    // Append draw cmd
    auto const geom = src.geometry;
    ctx.draw_cmds.push_back({
        .bindings{
            .vertex_buffers{
                geom->vertex,
                geom->vertex,
                geom->func,
            },
            .vertex_buffer_offsets{
                0,
                int(geom->vertex_count * sizeof(f32[3])),
                0,
            },
            .index_buffer = geom->index,
        },
        .pipeline = mat->pipeline(),
        .material = mat,
        .geometry = geom,
        .material_uniform_data = mat->uniform_data(),
        .object_uniform_slice{
            .start = slice_start,
            .size = sizeof(u),
        },
        .num_elements = int(geom->index_count),
        .num_instances = 1,
    });
}

void init_default_gfx_resources()
{
    Impl<ContourColorMaterial>::init_default_resources();
    Impl<ContourLineMaterial>::init_default_resources();
    // ...
}

void reload_default_shaders()
{
    Impl<ContourColorMaterial>::init_default_shader();
    Impl<ContourLineMaterial>::init_default_shader();
    // ...
}

} // namespace dr