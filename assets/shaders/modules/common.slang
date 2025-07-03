float luminance(float3 col)
{
    // https://en.wikipedia.org/wiki/Relative_luminance
    const float3 coeffs = { 0.2126, 0.7152, 0.0722 };
    return dot(col, coeffs);
}
