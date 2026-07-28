#version 330

layout(location = 0) in vec3 vertexPosition;

uniform mat4 matModel;
uniform mat4 matViewProjection;

out vec2 fragWorldXZ;

void main()
{
    vec4 worldPosition = matModel * vec4(vertexPosition, 1.0);
    fragWorldXZ = worldPosition.xz;
    gl_Position = matViewProjection * worldPosition;
}
