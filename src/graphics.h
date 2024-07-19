#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <sokol_gfx.h>

#ifdef __cplusplus
extern "C"
{
#endif

sg_shader_desc contour_color_shader_desc(char const* vs_src, char const* fs_src);

sg_pipeline_desc contour_color_pipeline_desc(sg_shader shader);

sg_shader_desc contour_line_shader_desc(char const* vs_src, char const* fs_src);

sg_pipeline_desc contour_line_pipeline_desc(sg_shader shader);

sg_buffer_desc vertex_buffer_desc(size_t size);

sg_buffer_desc index_buffer_desc(size_t size);

sg_image_desc matcap_image_desc(void const* data, int width, int height);

sg_sampler_desc matcap_sampler_desc(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GRAPHICS_H
