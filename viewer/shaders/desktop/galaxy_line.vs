#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;

uniform mat4 matViewProjection;
uniform vec3 _WorldSpaceCameraPos;

out vec2 fragTexCoord;
out float fragCameraDistance;

void main()
{
    gl_Position = matViewProjection * vec4(vertexPosition, 1.0);
    fragTexCoord = vertexTexCoord;
    fragCameraDistance = distance(_WorldSpaceCameraPos, vertexPosition);
}
