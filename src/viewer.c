#include "viewer.h"

#include <assert.h>

// NOTE(dr): The assigned shader stage doesn't appear to matter when using OpenGL backends
static sg_shader_stage const any_stage = SG_SHADERSTAGE_VERTEX;

sg_shader_desc contour_color_shader_desc(char const* const vs_src, char const* const fs_src)
{
    // clang-format off
    return (sg_shader_desc) {
        .vertex_func = {.source = vs_src},
        .fragment_func = {.source = fs_src},
        .uniform_blocks[UniformBlock_Material] = {
            .stage = any_stage,
            .size = sizeof(float[2]),
            .glsl_uniforms = {
                {.glsl_name = "u_spacing", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_offset", .type = SG_UNIFORMTYPE_FLOAT},
            },
        },
        .uniform_blocks[UniformBlock_Instance] = {
            .stage = any_stage,
            .size = sizeof(float[16 * 2]),
            .glsl_uniforms = {
                {.glsl_name = "u_local_to_clip", .type = SG_UNIFORMTYPE_MAT4},
                {.glsl_name = "u_local_to_view", .type = SG_UNIFORMTYPE_MAT4},
            },
        },
        .images[0] = {.stage = any_stage},
        .samplers[0] = {.stage = any_stage},
        .image_sampler_pairs[0] = {
            .glsl_name = "u_matcap", 
            .stage = any_stage, 
            .image_slot = 0, 
            .sampler_slot = 0,
        },
    };
    // clang-format on
}

sg_pipeline_desc contour_color_pipeline_desc(sg_shader const shader)
{
    // clang-format off
    return (sg_pipeline_desc) {
        .shader = shader,
        .layout = {
            .attrs = {
                {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT3},
                {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT3},
                {.buffer_index = 2, .format = SG_VERTEXFORMAT_FLOAT},
            },
        },
        .depth = {
            .compare = SG_COMPAREFUNC_LESS,
            .write_enabled = true,
        },
        .index_type = SG_INDEXTYPE_UINT32,
        .face_winding = SG_FACEWINDING_CCW,
    };
    // clang-format on
}

sg_shader_desc contour_line_shader_desc(char const* const vs_src, char const* const fs_src)
{
    // clang-format off
    return (sg_shader_desc) {
        .vertex_func = {.source = vs_src},
        .fragment_func = {.source = fs_src},
        .uniform_blocks[UniformBlock_Material] = {
            .stage = any_stage,
            .size = sizeof(float[3]),
            .glsl_uniforms = {
                {.glsl_name = "u_spacing", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_line_width", .type = SG_UNIFORMTYPE_FLOAT},
                {.glsl_name = "u_offset", .type = SG_UNIFORMTYPE_FLOAT},
            },
        },
        .uniform_blocks[UniformBlock_Instance] = {
            .stage = any_stage,
            .size = sizeof(float[16 * 2]),
            .glsl_uniforms = {
                {.glsl_name = "u_local_to_clip", .type = SG_UNIFORMTYPE_MAT4},
                {.glsl_name = "u_local_to_view", .type = SG_UNIFORMTYPE_MAT4},
            },
        },
    };
    // clang-format on
}

sg_pipeline_desc contour_line_pipeline_desc(sg_shader const shader)
{
    // clang-format off
    return (sg_pipeline_desc) {
        .shader = shader,
        .layout = {
            .attrs[0] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT3},
            .attrs[1] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT3},
            .attrs[2] = {.buffer_index = 2, .format = SG_VERTEXFORMAT_FLOAT},
        },
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = false,
        },
        .colors[0] = {
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        },
        .index_type = SG_INDEXTYPE_UINT32,
        .face_winding = SG_FACEWINDING_CCW,
    };
    // clang-format on
}

sg_buffer_desc mesh_vertex_buffer_desc(size_t const size)
{
    return (sg_buffer_desc){
        .size = size,
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_DYNAMIC,
    };
}

sg_buffer_desc mesh_index_buffer_desc(size_t const size)
{
    return (sg_buffer_desc){
        .size = size,
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_DYNAMIC,
    };
}

sg_buffer_desc mesh_plot_buffer_desc(size_t const size)
{
    return (sg_buffer_desc){
        .size = size,
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_DYNAMIC,
    };
}

sg_image_desc contour_color_matcap_image_desc(
    void const* const data,
    int const width,
    int const height)
{
    return (sg_image_desc){
        .width = width,
        .height = height,
        .usage = SG_USAGE_IMMUTABLE,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.subimage[0][0] =
            {
                .ptr = data,
                .size = width * height * 4,
            },
    };
}

sg_sampler_desc contour_color_matcap_sampler_desc(void)
{
    return (sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    };
}
