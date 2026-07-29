#version 300 es

layout(location = 0) in highp vec3 vertexPosition;
layout(location = 1) in highp vec2 vertexTexCoord;

uniform highp mat4 matModel;
uniform highp mat4 matViewProjection;
uniform highp vec4 _MainTex_ST;

out highp vec2 fragTexCoord;

void main()
{
    fragTexCoord = vertexTexCoord * _MainTex_ST.xy + _MainTex_ST.zw;
    gl_Position = matViewProjection * (matModel * vec4(vertexPosition, 1.0));
}
