#version 300 es

layout(location = 0) in highp vec3 vertexPosition;
layout(location = 1) in highp vec2 vertexTexCoord;

uniform highp mat4 matViewProjection;
uniform highp vec3 _WorldSpaceCameraPos;

out highp vec2 fragTexCoord;
out highp float fragCameraDistance;

void main()
{
    gl_Position = matViewProjection * vec4(vertexPosition, 1.0);
    fragTexCoord = vertexTexCoord;
    fragCameraDistance = distance(_WorldSpaceCameraPos, vertexPosition);
}
