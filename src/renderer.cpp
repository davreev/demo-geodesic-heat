#include "renderer.hpp"

#include <dr/linalg_reshape.hpp>
#include <dr/memory.hpp>

#include "assets.hpp"
#include "utils.hpp"

namespace dr
{
namespace
{

// NOTE(dr): The assigned shader stage doesn't appear to matter when using OpenGL backends
static sg_shader_stage const shader_stage_any = SG_SHADERSTAGE_VERTEX;

// NOTE(dr): Not using standard ctors on param types so that they still qualify for
// aggregate/designated init

struct PassParams
{
    f32 world_to_view[16]{};
    f32 world_to_clip[16]{};

    static PassParams make(Mat4<f32> const& world_to_view, Mat4<f32> const& view_to_clip)
    {
        PassParams p;
        as_mat<4, 4>(p.world_to_view) = world_to_view;
        as_mat<4, 4>(p.world_to_clip) = view_to_clip * world_to_view;
        return p;
    }

    static sg_shader_uniform_block uniform_block()
    {
        return {
            .stage = shader_stage_any,
            .size = sizeof(PassParams),
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
};

struct ObjectParams
{
    f32 local_to_world[16]{};

    static ObjectParams make(Mat4<f32> const& local_to_world)
    {
        ObjectParams p;
        as_mat<4, 4>(p.local_to_world) = local_to_world;
        return p;
    }

    static sg_shader_uniform_block uniform_block()
    {
        return {
            .stage = shader_stage_any,
            .size = sizeof(ObjectParams),
            .glsl_uniforms{
                {
                    .type = SG_UNIFORMTYPE_FLOAT4,
                    .array_count = 4,
                    .glsl_name = "object.local_to_world.data",
                },
            },
        };
    }
};

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
        GfxView view;
        GfxSampler sampler;
    } inline static default_matcap;

    struct Params
    {
        f32 spacing{};
        f32 offset{};

        static Params make(ContourColorMaterial const& src)
        {
            return {
                .spacing = src.spacing,
                .offset = src.offset,
            };
        }

        static sg_shader_uniform_block uniform_block()
        {
            return {
                .stage = shader_stage_any,
                .size = sizeof(Params),
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
            };
        }
    };

    static void init_default_shader()
    {
        ShaderAsset const* vs = get_asset(AssetHandle::Shader_ContourColorVert, true);
        assert(vs);

        ShaderAsset const* fs = get_asset(AssetHandle::Shader_ContourColorFrag, true);
        assert(fs);

        default_shader.init({
            .vertex_func{.source = vs->src.c_str()},
            .fragment_func{.source = fs->src.c_str()},
            .uniform_blocks{
                PassParams::uniform_block(),
                Params::uniform_block(),
                {}, // Geometry block (unused)
                ObjectParams::uniform_block(),
            },
            .views{
                {.texture{.stage = shader_stage_any}},
            },
            .samplers{
                {.stage = shader_stage_any},
            },
            .texture_sampler_pairs{
                {
                    .stage = shader_stage_any,
                    .view_slot = 0,
                    .sampler_slot = 0,
                    .glsl_name = "matcap",
                },
            },
        });
        assert(default_shader.is_valid());
    };

    static void init_default_resources()
    {
        assert(!default_pipeline.is_valid());

        default_shader = GfxShader::alloc();
        init_default_shader();

        default_pipeline = GfxPipeline::make({
            .shader = default_shader,
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
        });
        assert(default_pipeline.is_valid());

        {
            ImageAsset const* image = get_asset(AssetHandle::Image_Matcap);
            assert(image);

            default_matcap.image = GfxImage::make({
                .usage = {.immutable = true},
                .width = int(image->width),
                .height = int(image->height),
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .data{
                    .mip_levels{
                        {.ptr = image->data.get(), .size = usize(image->width * image->height * 4)},
                    },
                },
            });
            assert(default_matcap.image.is_valid());

            default_matcap.view = GfxView::make({
                .texture{.image = default_matcap.image},
            });
            assert(default_matcap.view.is_valid());

            default_matcap.sampler = GfxSampler::make({
                .min_filter = SG_FILTER_LINEAR,
                .mag_filter = SG_FILTER_LINEAR,
            });
            assert(default_matcap.sampler.is_valid());
        }
    };
};

template <>
struct Impl<ContourLineMaterial>
{
    inline static GfxPipeline default_pipeline;
    inline static GfxShader default_shader;

    struct Params
    {
        f32 spacing{};
        f32 offset{};
        f32 line_width{};

        static Params make(ContourLineMaterial const& src)
        {
            return {
                .spacing = src.spacing,
                .offset = src.offset,
                .line_width = src.line_width,
            };
        }

        static sg_shader_uniform_block uniform_block()
        {
            return {
                .stage = shader_stage_any,
                .size = sizeof(Params),
                .glsl_uniforms{
                    {
                        .type = SG_UNIFORMTYPE_FLOAT,
                        .glsl_name = "material.spacing",
                    },
                    {
                        .type = SG_UNIFORMTYPE_FLOAT,
                        .glsl_name = "material.offset",
                    },
                    {
                        .type = SG_UNIFORMTYPE_FLOAT,
                        .glsl_name = "material.line_width",
                    },
                },
            };
        }
    };

    static void init_default_shader()
    {
        ShaderAsset const* vs = get_asset(AssetHandle::Shader_ContourLineVert, true);
        assert(vs);

        ShaderAsset const* fs = get_asset(AssetHandle::Shader_ContourLineFrag, true);
        assert(fs);

        default_shader.init({
            .vertex_func{.source = vs->src.c_str()},
            .fragment_func{.source = fs->src.c_str()},
            .uniform_blocks{
                PassParams::uniform_block(),
                Params::uniform_block(),
                {}, // Geometry block (unused)
                ObjectParams::uniform_block(),
            },
        });
        assert(default_shader.is_valid());
    };

    static void init_default_resources()
    {
        assert(!default_pipeline.is_valid());

        default_shader = GfxShader::alloc();
        init_default_shader();

        default_pipeline = GfxPipeline::make({
            .shader = default_shader,
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
        });
        assert(default_pipeline.is_valid());
    };
};

} // namespace

GfxPipeline::Handle ContourColorMaterial::pipeline() const
{
    return Impl<ContourColorMaterial>::default_pipeline;
}

GfxPipeline::Handle ContourLineMaterial::pipeline() const
{
    return Impl<ContourLineMaterial>::default_pipeline;
}

void Renderer::init_default_resources()
{
    Impl<ContourColorMaterial>::init_default_resources();
    Impl<ContourLineMaterial>::init_default_resources();
    // ...
}

void Renderer::reload_default_shaders()
{
    Impl<ContourColorMaterial>::init_default_shader();
    Impl<ContourLineMaterial>::init_default_shader();
    // ...
}

void Renderer::render(SceneView const& scene, DrawContext& draw_ctx)
{
    auto const pass_params = PassParams::make(
        scene.camera.world_to_view,
        scene.camera.view_to_clip);

    // Mesh plots (opaque)
    {
        for (auto const& mp : scene.mesh_plots)
            emit_draw_cmds<ContourColorMaterial>(mp, draw_ctx);

        draw_ctx.submit_draw_cmds({
            .uniform_data = as_bytes(pass_params),
        });
    }

    // Mesh plots (transparent)
    {
        for (auto const& mp : scene.mesh_plots)
            emit_draw_cmds<ContourLineMaterial>(mp, draw_ctx);

        draw_ctx.submit_draw_cmds({
            .uniform_data = as_bytes(pass_params),
        });
    }
}

template <>
void emit_draw_cmds<ContourColorMaterial>(MeshPlot const& src, DrawContext& draw_ctx)
{
    using Material = ContourColorMaterial;
    using Geometry = MeshPlotGeometry;

    auto const mat = src.materials.contour_color;
    if (mat == nullptr)
        return;

    auto set_bindings = [](DrawCommand const& cmd, GfxBindings& b) {
        auto const geom = static_cast<Geometry const*>(cmd.geometry);
        b.vertex_buffers[0] = geom->vertex;
        b.vertex_buffers[1] = geom->vertex;
        b.vertex_buffers[2] = geom->func;
        b.vertex_buffer_offsets[0] = 0;
        b.vertex_buffer_offsets[1] = geom->vertex_count * sizeof(f32[3]);
        b.vertex_buffer_offsets[2] = 0;
        b.index_buffer = geom->index;

        auto const mat = static_cast<Material const*>(cmd.material);
        b.views[0] = valid_or(mat->matcap.view, Impl<Material>::default_matcap.view.handle());
        b.samplers[0] = valid_or(
            mat->matcap.sampler,
            Impl<Material>::default_matcap.sampler.handle());
    };

    auto const mat_params = Impl<Material>::Params::make(*mat);
    auto const obj_params = ObjectParams::make(src.transform.to_matrix());

    draw_ctx.draw_cmds.push_back({
        .pipeline = mat->pipeline(),
        .material = mat,
        .geometry = src.geometry,
        .set_bindings = set_bindings,
        .uniform_slices{
            .material = draw_ctx.push_uniforms_once(mat, as_bytes(mat_params)),
            .object = draw_ctx.push_uniforms(as_bytes(obj_params)),
        },
        .num_elements = int(src.geometry->index_count),
        .num_instances = 1,
    });
}

template <>
void emit_draw_cmds<ContourLineMaterial>(MeshPlot const& src, DrawContext& draw_ctx)
{
    using Material = ContourLineMaterial;
    using Geometry = MeshPlotGeometry;

    auto const mat = src.materials.contour_line;
    if (mat == nullptr)
        return;

    auto set_bindings = [](DrawCommand const& cmd, GfxBindings& b) {
        auto const geom = static_cast<Geometry const*>(cmd.geometry);
        b.vertex_buffers[0] = geom->vertex;
        b.vertex_buffers[1] = geom->vertex;
        b.vertex_buffers[2] = geom->func;
        b.vertex_buffer_offsets[0] = 0;
        b.vertex_buffer_offsets[1] = geom->vertex_count * sizeof(f32[3]);
        b.vertex_buffer_offsets[2] = 0;
        b.index_buffer = geom->index;
    };

    auto const mat_params = Impl<Material>::Params::make(*mat);
    auto const obj_params = ObjectParams::make(src.transform.to_matrix());

    draw_ctx.draw_cmds.push_back({
        .pipeline = mat->pipeline(),
        .material = mat,
        .geometry = src.geometry,
        .set_bindings = set_bindings,
        .uniform_slices{
            .material = draw_ctx.push_uniforms_once(mat, as_bytes(mat_params)),
            .object = draw_ctx.push_uniforms(as_bytes(obj_params)),
        },
        .num_elements = int(src.geometry->index_count),
        .num_instances = 1,
    });
}

} // namespace dr