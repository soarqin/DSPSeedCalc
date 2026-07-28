#version 300 es
precision highp float;

in highp vec2 fragTexCoord;

uniform sampler2D texture0;
uniform highp vec2 sourceTexelSize;

out highp vec4 finalColor;

void AddSample(inout highp vec3 sum, inout highp float weightSum, highp vec2 uv)
{
    highp vec3 color = texture(texture0, uv).rgb;
    highp float weight = 1.0 / (max(color.r, max(color.g, color.b)) + 1.0);
    sum += color * weight;
    weightSum += weight;
}

void main()
{
    highp vec3 sum = vec3(0.0);
    highp float weightSum = 0.0;
    AddSample(sum, weightSum, fragTexCoord + sourceTexelSize * vec2(-1.0, -1.0));
    AddSample(sum, weightSum, fragTexCoord + sourceTexelSize * vec2( 1.0, -1.0));
    AddSample(sum, weightSum, fragTexCoord + sourceTexelSize * vec2(-1.0,  1.0));
    AddSample(sum, weightSum, fragTexCoord + sourceTexelSize * vec2( 1.0,  1.0));
    finalColor = vec4(sum / weightSum, 0.0);
}
