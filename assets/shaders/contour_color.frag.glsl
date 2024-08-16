#version 330 core

uniform sampler2D u_matcap;

uniform float u_spacing;
uniform float u_offset;

in vec3 v_view_normal;
in float v_scalar;

out vec4 f_color;

float luminance(vec3 col)
{
    // https://en.wikipedia.org/wiki/Relative_luminance
    const vec3 coeffs = vec3(0.2126, 0.7152, 0.0722);
    return dot(col, coeffs);
}

vec3 matcap_shade(vec3 base_color, vec3 view_normal, float intensity, float neutral)
{
    vec2 uv = view_normal.xy * 0.5 + 0.5;
    float offset = luminance(textureLod(u_matcap, uv, 0.0).rgb) - neutral;
    return base_color + offset * intensity;
}

void main()
{
    float t = fract((v_scalar + u_offset) / u_spacing);
    vec3 view_norm = normalize(v_view_normal);
    vec3 base_col = vec3(t, view_norm.x * 0.5 + 0.5, 0.5);

    vec3 col = matcap_shade(base_col, view_norm, 1.0, 0.9);
    if (gl_FrontFacing)
        col *= 0.2;

    f_color = vec4(col, 1.0);
}
