#version 330 core

uniform float u_spacing;
uniform float u_width;
uniform float u_offset;

in vec3 v_view_position;
in vec3 v_view_normal;
in float v_scalar;

out vec4 f_color;

vec4 shade_rim(vec3 view_dir, vec3 view_norm, vec4 color, vec4 color_base)
{
    float t = 1.0 - abs(dot(view_dir, view_norm));
    return mix(color_base, color, t * t * t);
}

vec4 draw_contour(
    float f,
    float width,
    vec4 color,
    vec4 color_base)
{
    float df = fwidth(f);
    float t = abs(fract(f + 0.5) - 0.5);
    const float eps = 0.5;
    return mix(color, color_base, smoothstep((width - eps) * df, (width + eps) * df, t));
}

void main() 
{
    vec4 col = vec4(vec3(0.85), 0.0);

    col = shade_rim(
        normalize(v_view_position), 
        v_view_normal, 
        vec4(vec3(0.85), 0.2),
        col);

    float f = (v_scalar + u_offset) / u_spacing;
    
    col = draw_contour(
        4.0 * f,
        u_width,
        vec4(vec3(0.85), gl_FrontFacing? 0.1 : 0.5), 
        col);

    col = draw_contour(
        f,
        1.5 * u_width,
        vec4(vec3(0.85), gl_FrontFacing? 0.2 : 1.0), 
        col);

    f_color = col;
}
