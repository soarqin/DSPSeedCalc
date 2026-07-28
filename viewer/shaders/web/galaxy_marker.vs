#version 300 es
precision highp float;
precision highp int;

layout(location = 0) in highp vec3 vertexPosition;
layout(location = 1) in highp vec2 vertexTexCoord;

uniform highp mat4 matModel;
uniform highp mat4 matViewProjection;

out highp vec2 fragTexCoord;

void main()
{
    fragTexCoord = vertexTexCoord;
    gl_Position = matViewProjection * (matModel * vec4(vertexPosition, 1.0));
}
