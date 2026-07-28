#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 sourceTexelSize;
uniform float prefilterOffset;
uniform float threshold;
uniform vec3 curve;

out vec4 finalColor;

vec3 SafeHdr(vec3 color)
{
    return min(color, vec3(65504.0));
}

vec3 Median(vec3 a, vec3 b, vec3 c)
{
    return a + b + c - min(min(a, b), c) - max(max(a, b), c);
}

void main()
{
    vec2 uv = fragTexCoord + sourceTexelSize * prefilterOffset;
    vec3 center = SafeHdr(texture(texture0, uv).rgb);
    vec3 left = SafeHdr(texture(texture0, uv - vec2(sourceTexelSize.x, 0.0)).rgb);
    vec3 right = SafeHdr(texture(texture0, uv + vec2(sourceTexelSize.x, 0.0)).rgb);
    vec3 down = SafeHdr(texture(texture0, uv - vec2(0.0, sourceTexelSize.y)).rgb);
    vec3 up = SafeHdr(texture(texture0, uv + vec2(0.0, sourceTexelSize.y)).rgb);
    vec3 color = Median(Median(center, left, right), down, up);
    float brightness = max(color.r, max(color.g, color.b));
    float soft = clamp(brightness - curve.x, 0.0, curve.y);
    soft = curve.z * soft * soft;
    float contribution = max(brightness - threshold, soft) / max(brightness, 0.00001);
    finalColor = vec4(color * contribution, 0.0);
}
