#include "graphics.h"

#include <assert.h>

sg_shader_desc contour_color_shader_desc(char const* const vs_src, char const* const fs_src)
{
    // clang-format off
    return (sg_shader_desc) {
        .vs = {
            .source = vs_src,
            .uniform_blocks[0] = {
                .uniforms[0] = {.name = "u_local_to_clip", .type = SG_UNIFORMTYPE_MAT4},
                .uniforms[1] = {.name = "u_local_to_view", .type = SG_UNIFORMTYPE_MAT4},
                .size = 16 * 2 * sizeof(float),
            },
        },
        .fs = {
            .source = fs_src,
            .uniform_blocks[0] = {
                .uniforms[0] = {.name = "u_spacing", .type = SG_UNIFORMTYPE_FLOAT},
                .uniforms[1] = {.name = "u_offset", .type = SG_UNIFORMTYPE_FLOAT},
                .uniforms[2] = {.name = "u_time", .type = SG_UNIFORMTYPE_FLOAT},
                .size = 3 * sizeof(float),
            },
            .images[0] = {.used = true},
            .samplers[0] = {.used = true},
            .image_sampler_pairs[0] = {
                .used = true, 
                .image_slot = 0, 
                .sampler_slot = 0,
                .glsl_name = "u_matcap", 
            },
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
            .attrs[0] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT3},
            .attrs[1] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_FLOAT3},
            .attrs[2] = {.buffer_index = 2, .format = SG_VERTEXFORMAT_FLOAT},
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
        .vs = {
            .source = vs_src,
            .uniform_blocks[0] = {
                .uniforms[0] = {.name = "u_local_to_clip", .type = SG_UNIFORMTYPE_MAT4},
                .uniforms[1] = {.name = "u_local_to_view", .type = SG_UNIFORMTYPE_MAT4},
                .size = 16 * 2 * sizeof(float),
            },
        },
        .fs = {
            .source = fs_src,
            .uniform_blocks[0] = {
                .uniforms[0] = {.name = "u_spacing", .type = SG_UNIFORMTYPE_FLOAT},
                .uniforms[1] = {.name = "u_width", .type = SG_UNIFORMTYPE_FLOAT},
                .uniforms[2] = {.name = "u_offset", .type = SG_UNIFORMTYPE_FLOAT},
                .size = 3 * sizeof(float),
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
            .compare = SG_COMPAREFUNC_ALWAYS,
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

sg_buffer_desc vertex_buffer_desc(size_t const size)
{
    return (sg_buffer_desc){
        .size = size,
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_DYNAMIC,
    };
}

sg_buffer_desc index_buffer_desc(size_t const size)
{
    return (sg_buffer_desc){
        .size = size,
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_DYNAMIC,
    };
}

sg_image_desc matcap_image_desc(void const* const data, int const width, int const height)
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

sg_sampler_desc matcap_sampler_desc(void)
{
    return (sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
    };
}
