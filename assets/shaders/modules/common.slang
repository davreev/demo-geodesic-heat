float luminance(float3 col)
{
    // https://en.wikipedia.org/wiki/Relative_luminance
    const float3 coeffs = { 0.2126, 0.7152, 0.0722 };
    return dot(col, coeffs);
}

float4 sample_matcap(Sampler2D sampler, float3 view_normal)
{
    float2 uv = view_normal.xy * 0.5 + 0.5;
    return sampler.SampleLevel(uv, 0.0);
}

void apply_matcap_shading(
    Sampler2D sampler,
    float3 view_normal,
    float intensity,
    float neutral,
    inout float4 col)
{
    float bright = luminance(sample_matcap(sampler, view_normal).rgb);
    col += (bright - neutral) * intensity;
}

float4 alpha_blend(float4 src, float4 dst)
{
    return { lerp(dst.rgb, src.rgb, src.a), src.a + dst.a * (1.0 - src.a) };
}

float draw_contour(float f, float width)
{
    float df = fwidth(f);
    float t = abs(frac(f + 0.5) - 0.5);
    const float h = 0.5;
    return smoothstep((width + h) * df, (width - h) * df, t);
}

float4 draw_contour(float f, float width, float4 color, float4 color_base)
{
    color.a *= draw_contour(f, width);
    return alpha_blend(color_base, color);
}
