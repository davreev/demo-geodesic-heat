#version 330 core

uniform sampler2D u_matcap;

uniform float u_spacing;
uniform float u_offset;
uniform float u_time;

in vec3 v_view_normal;
in float v_scalar;

out vec4 f_color;

const float pi = 3.1415926538;
const float two_pi = 2.0 * pi;

vec4 matcap_shade(vec3 base_color, vec3 view_normal, float intensity, float neutral)
{
    vec2 uv = view_normal.xy * 0.5 + 0.5;
    float offset = textureLod(u_matcap, uv, 0.0).r - neutral;
    return vec4(base_color + offset * intensity, 1.0);
}

void main() 
{
    float t = fract((v_scalar + u_offset) / u_spacing);

    const float inv_period = 0.2;
    float pulse = sin(u_time * inv_period * two_pi);

    vec3 base_col = vec3(
        t + pulse * 0.3,
        0.7,
        (1.0 - t * 0.7) + pulse * 0.3
    );
    f_color = matcap_shade(base_col, v_view_normal, 1.2, 0.85);
}
