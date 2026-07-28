#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 sourceTexelSize;

out vec4 finalColor;

void main()
{
    vec3 color = texture(texture0, fragTexCoord + sourceTexelSize * vec2(-1.0, -1.0)).rgb;
    color += texture(texture0, fragTexCoord + sourceTexelSize * vec2( 1.0, -1.0)).rgb;
    color += texture(texture0, fragTexCoord + sourceTexelSize * vec2(-1.0,  1.0)).rgb;
    color += texture(texture0, fragTexCoord + sourceTexelSize * vec2( 1.0,  1.0)).rgb;
    finalColor = vec4(color * 0.25, 0.0);
}
