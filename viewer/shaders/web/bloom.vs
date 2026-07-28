#version 300 es

layout(location = 0) in highp vec3 vertexPosition;
layout(location = 1) in highp vec2 vertexTexCoord;

uniform highp mat4 mvp;

out highp vec2 fragTexCoord;

void main()
{
    gl_Position = mvp * vec4(vertexPosition, 1.0);
    fragTexCoord = vertexTexCoord;
}
