#version 330 core

uniform sampler2D u_matcap;

uniform float u_spacing;
uniform float u_offset;

in vec3 v_view_normal;
in float v_scalar;

out vec4 f_color;

vec4 matcap_shade(vec3 base_color, vec3 view_normal, float intensity, float neutral)
{
    vec2 uv = view_normal.xy * 0.5 + 0.5;
    float offset = texture(u_matcap, uv).r - neutral;
    return vec4(base_color + offset * intensity, 1.0);
}

void main() 
{
    float t = fract((v_scalar + u_offset) / u_spacing);
    vec3 base_col = vec3(t, 0.8, 1.0 - t);
    f_color = matcap_shade(base_col, v_view_normal, 1.2, 0.85);
}
