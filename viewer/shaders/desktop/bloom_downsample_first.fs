#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 sourceTexelSize;

out vec4 finalColor;

void AddSample(inout vec3 sum, inout float weightSum, vec2 uv)
{
    vec3 color = texture(texture0, uv).rgb;
    float weight = 1.0 / (max(color.r, max(color.g, color.b)) + 1.0);
    sum += color * weight;
    weightSum += weight;
}

void main()
{
    vec3 sum = vec3(0.0);
    float weightSum = 0.0;
    AddSample(sum, weightSum, fragTexCoord + sourceTexelSize * vec2(-1.0, -1.0));
    AddSample(sum, weightSum, fragTexCoord + sourceTexelSize * vec2( 1.0, -1.0));
    AddSample(sum, weightSum, fragTexCoord + sourceTexelSize * vec2(-1.0,  1.0));
    AddSample(sum, weightSum, fragTexCoord + sourceTexelSize * vec2( 1.0,  1.0));
    finalColor = vec4(sum / weightSum, 0.0);
}
