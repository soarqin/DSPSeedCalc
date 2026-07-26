#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform sampler2D lensDirtTexture;
uniform sampler2D gradingLutTexture;
uniform vec2 sourceTexelSize;
uniform float intensity;
uniform float dirtIntensity;

out vec4 finalColor;

vec3 TentFilter(vec2 uv)
{
    vec2 d = sourceTexelSize;
    vec3 color = texture(bloomTexture, uv + vec2(-d.x, -d.y)).rgb;
    color += texture(bloomTexture, uv + vec2( d.x, -d.y)).rgb;
    color += texture(bloomTexture, uv + vec2(-d.x,  d.y)).rgb;
    color += texture(bloomTexture, uv + vec2( d.x,  d.y)).rgb;
    color += texture(bloomTexture, uv + vec2(-d.x, 0.0)).rgb * 2.0;
    color += texture(bloomTexture, uv + vec2( d.x, 0.0)).rgb * 2.0;
    color += texture(bloomTexture, uv + vec2(0.0, -d.y)).rgb * 2.0;
    color += texture(bloomTexture, uv + vec2(0.0,  d.y)).rgb * 2.0;
    color += texture(bloomTexture, uv).rgb * 4.0;
    return color * 0.0625;
}

vec3 LinearToLogC(vec3 color)
{
    return 0.244161 * log2(5.555556 * color + 0.047996) / log2(10.0) + 0.386036;
}

vec3 LinearToSrgb(vec3 value)
{
    value = max(value, vec3(0.0));
    vec3 low = value * 12.92;
    vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

vec3 ApplyLut2d(vec3 uvw)
{
    const vec3 scaleOffset = vec3(1.0 / 1024.0, 1.0 / 32.0, 31.0);
    uvw.z *= scaleOffset.z;
    float shift = floor(uvw.z);
    uvw.xy = uvw.xy * scaleOffset.z * scaleOffset.xy + scaleOffset.xy * 0.5;
    uvw.x += shift * scaleOffset.y;
    vec3 a = texture(gradingLutTexture, uvw.xy).rgb;
    vec3 b = texture(gradingLutTexture, uvw.xy + vec2(scaleOffset.y, 0.0)).rgb;
    return mix(a, b, uvw.z - shift);
}

void main()
{
    vec3 scene = texture(texture0, fragTexCoord).rgb;
    vec3 bloom = TentFilter(fragTexCoord) * intensity;
    vec3 dirt = texture(lensDirtTexture, vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)).rgb * dirtIntensity;
    vec3 color = scene + bloom + bloom * dirt;
    vec3 logColor = clamp(LinearToLogC(max(color, vec3(0.0))), 0.0, 1.0);
    finalColor = vec4(LinearToSrgb(clamp(ApplyLut2d(logColor), 0.0, 1.0)), 1.0);
}
