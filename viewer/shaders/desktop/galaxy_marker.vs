#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;

uniform mat4 matModel;
uniform mat4 matViewProjection;

out vec2 fragTexCoord;

void main()
{
    fragTexCoord = vertexTexCoord;
    gl_Position = matViewProjection * (matModel * vec4(vertexPosition, 1.0));
}
