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
struct Params;

template <>
struct Params<ContourColorMaterial>
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

template <>
struct Params<ContourLineMaterial>
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
                PassParams::uniform_block(),
                Params<ContourColorMaterial>::uniform_block(),
                {}, // Geometry block (unused)
                ObjectParams::uniform_block(),
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
                PassParams::uniform_block(),
                Params<ContourLineMaterial>::uniform_block(),
                {}, // Geometry block (unused)
                ObjectParams::uniform_block(),
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

} // namespace

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

GfxPipeline::Handle ContourColorMaterial::pipeline() const
{
    return Impl<ContourColorMaterial>::default_pipeline;
}

GfxPipeline::Handle ContourLineMaterial::pipeline() const
{
    return Impl<ContourLineMaterial>::default_pipeline;
}

template <>
void Renderer::render(SceneDesc const& scene)
{
    auto begin_pass = [&]() {
        draw_cmds_.clear();
        uniform_data_.clear();
    };

    auto end_pass = [&]() {
        order_draw_cmds(as_span(draw_cmds_));
        submit_draw_cmds(as_span(draw_cmds_), uniform_data_);
    };

    auto const params = PassParams::make(scene.camera.world_to_view, scene.camera.view_to_clip);

    // Mesh plots (opaque)
    {
        begin_pass();
        uniform_data_.push_back(as_bytes(params));

        for (auto const& mp : scene.mesh_plots)
            emit_draw_cmds<ContourColorMaterial>(mp, draw_cmds_, uniform_data_);

        end_pass();
    }

    // Mesh plots (transparent)
    {
        begin_pass();
        uniform_data_.push_back(as_bytes(params));

        for (auto const& mp : scene.mesh_plots)
            emit_draw_cmds<ContourLineMaterial>(mp, draw_cmds_, uniform_data_);

        end_pass();
    }
}

template <>
void emit_draw_cmds<ContourColorMaterial>(
    MeshPlot const& src,
    DynamicArray<DrawCommand>& draw_cmds,
    SlicedArray<u8>& uniform_data)
{
    auto const mat = src.materials.contour_color;

    // Skip if material isn't assigned
    if (mat == nullptr)
        return;

    using Material = ContourColorMaterial;
    using Geometry = MeshPlotGeometry;

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
        b.images[0] = valid_or(mat->matcap.image, Impl<Material>::default_matcap.image.handle());
        b.samplers[0] = valid_or(
            mat->matcap.sampler,
            Impl<Material>::default_matcap.sampler.handle());
    };

    // Append draw cmd
    draw_cmds.push_back({
        .pipeline = mat->pipeline(),
        .material = mat,
        .geometry = src.geometry,
        .set_bindings = set_bindings,
        .uniform_slices{
            .material = uniform_data.num_slices(),
            .object = uniform_data.num_slices() + 1,
        },
        .num_elements = int(src.geometry->index_count),
        .num_instances = 1,
    });

    // Append material uniforms
    auto const mat_params = Params<Material>::make(*mat);
    uniform_data.push_back(as_bytes(mat_params));

    // Append object uniforms
    auto const obj_params = ObjectParams::make(src.transform.to_matrix());
    uniform_data.push_back(as_bytes(obj_params));
}

template <>
void emit_draw_cmds<ContourLineMaterial>(
    MeshPlot const& src,
    DynamicArray<DrawCommand>& draw_cmds,
    SlicedArray<u8>& uniform_data)
{
    auto const mat = src.materials.contour_line;

    // Skip if material isn't assigned
    if (mat == nullptr)
        return;

    using Material = ContourLineMaterial;
    using Geometry = MeshPlotGeometry;

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

    // Append draw cmd
    draw_cmds.push_back({
        .pipeline = mat->pipeline(),
        .material = mat,
        .geometry = src.geometry,
        .set_bindings = set_bindings,
        .uniform_slices{
            .material = uniform_data.num_slices(),
            .object = uniform_data.num_slices() + 1,
        },
        .num_elements = int(src.geometry->index_count),
        .num_instances = 1,
    });

    // Append material uniforms
    auto const mat_params = Params<Material>::make(*mat);
    uniform_data.push_back(as_bytes(mat_params));

    // Append object uniforms
    auto const obj_params = ObjectParams::make(src.transform.to_matrix());
    uniform_data.push_back(as_bytes(obj_params));
}

} // namespace dr