#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform sampler2D baseTexture;
uniform vec2 sourceTexelSize;

out vec4 finalColor;

vec3 TentFilter(vec2 uv)
{
    vec2 d = sourceTexelSize;
    vec3 color = texture(texture0, uv + vec2(-d.x, -d.y)).rgb;
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
