#version 300 es
precision highp float;

in highp vec2 fragTexCoord;

uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform sampler2D lensDirtTexture;
uniform sampler2D gradingLutTexture;
uniform highp vec2 sourceTexelSize;
uniform highp float intensity;
uniform highp float dirtIntensity;

out highp vec4 finalColor;

highp vec3 TentFilter(highp vec2 uv)
{
    highp vec2 d = sourceTexelSize;
    highp vec3 color = texture(bloomTexture, uv + vec2(-d.x, -d.y)).rgb;
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

highp vec3 LinearToLogC(highp vec3 color)
{
    return 0.244161 * log2(5.555556 * color + 0.047996) / log2(10.0) + 0.386036;
}

highp vec3 ApplyLut2d(highp vec3 uvw)
{
    const highp vec3 scaleOffset = vec3(1.0 / 1024.0, 1.0 / 32.0, 31.0);
    uvw.z *= scaleOffset.z;
    highp float shift = floor(uvw.z);
    uvw.xy = uvw.xy * scaleOffset.z * scaleOffset.xy + scaleOffset.xy * 0.5;
    uvw.x += shift * scaleOffset.y;
    highp vec3 a = texture(gradingLutTexture, uvw.xy).rgb;
    highp vec3 b = texture(gradingLutTexture, uvw.xy + vec2(scaleOffset.y, 0.0)).rgb;
    return mix(a, b, uvw.z - shift);
}

void main()
{
    highp vec3 scene = texture(texture0, fragTexCoord).rgb;
    highp vec3 bloom = TentFilter(fragTexCoord) * intensity;
    highp vec3 dirt = texture(lensDirtTexture, vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)).rgb * dirtIntensity;
    highp vec3 color = scene + bloom + bloom * dirt;
    highp vec3 logColor = clamp(LinearToLogC(max(color, vec3(0.0))), 0.0, 1.0);
    finalColor = vec4(clamp(ApplyLut2d(logColor), 0.0, 1.0), 1.0);
}
