#version 300 es
precision highp float;

in highp vec2 fragTexCoord;

uniform sampler2D texture0;
uniform highp vec2 sourceTexelSize;
uniform highp float prefilterOffset;
uniform highp float threshold;
uniform highp vec3 curve;

out highp vec4 finalColor;

highp vec3 SafeHdr(highp vec3 color)
{
    return min(color, vec3(65504.0));
}

highp vec3 Median(highp vec3 a, highp vec3 b, highp vec3 c)
{
    return a + b + c - min(min(a, b), c) - max(max(a, b), c);
}

void main()
{
    highp vec2 uv = fragTexCoord + sourceTexelSize * prefilterOffset;
    highp vec3 center = SafeHdr(texture(texture0, uv).rgb);
    highp vec3 left = SafeHdr(texture(texture0, uv - vec2(sourceTexelSize.x, 0.0)).rgb);
    highp vec3 right = SafeHdr(texture(texture0, uv + vec2(sourceTexelSize.x, 0.0)).rgb);
    highp vec3 down = SafeHdr(texture(texture0, uv - vec2(0.0, sourceTexelSize.y)).rgb);
    highp vec3 up = SafeHdr(texture(texture0, uv + vec2(0.0, sourceTexelSize.y)).rgb);
    highp vec3 color = Median(Median(center, left, right), down, up);
    highp float brightness = max(color.r, max(color.g, color.b));
    highp float soft = clamp(brightness - curve.x, 0.0, curve.y);
    soft = curve.z * soft * soft;
    highp float contribution = max(brightness - threshold, soft) / max(brightness, 0.00001);
    finalColor = vec4(color * contribution, 0.0);
}
