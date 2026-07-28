#version 300 es
precision highp float;

in highp vec2 fragTexCoord;

uniform sampler2D texture0;
uniform sampler2D baseTexture;
uniform highp vec2 sourceTexelSize;

out highp vec4 finalColor;

highp vec3 TentFilter(highp vec2 uv)
{
    highp vec2 d = sourceTexelSize;
    highp vec3 color = texture(texture0, uv + vec2(-d.x, -d.y)).rgb;
    color += texture(texture0, uv + vec2( d.x, -d.y)).rgb;
    color += texture(texture0, uv + vec2(-d.x,  d.y)).rgb;
    color += texture(texture0, uv + vec2( d.x,  d.y)).rgb;
    color += texture(texture0, uv + vec2(-d.x, 0.0)).rgb * 2.0;
    color += texture(texture0, uv + vec2( d.x, 0.0)).rgb * 2.0;
    color += texture(texture0, uv + vec2(0.0, -d.y)).rgb * 2.0;
    color += texture(texture0, uv + vec2(0.0,  d.y)).rgb * 2.0;
    color += texture(texture0, uv).rgb * 4.0;
    return color * 0.0625;
}

void main()
{
    finalColor = vec4(texture(baseTexture, fragTexCoord).rgb + TentFilter(fragTexCoord), 0.0);
}
