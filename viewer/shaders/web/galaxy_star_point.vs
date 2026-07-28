#version 300 es

layout(location = 0) in highp vec3 vertexPosition;
layout(location = 1) in highp vec2 vertexTexCoord;
layout(location = 3) in lowp vec4 vertexColor;

uniform highp mat4 matModel;
uniform highp mat4 matViewProjection;
uniform lowp vec4 colDiffuse;
uniform highp vec4 _MainTex_ST;

out lowp vec4 fragColor;
out highp vec2 fragTexCoord;

void main()
{
    highp vec4 worldPosition = matModel * vec4(vertexPosition, 1.0);
    gl_Position = matViewProjection * worldPosition;
    fragColor = vec4(1.0);
    fragTexCoord = vertexTexCoord * _MainTex_ST.xy + _MainTex_ST.zw;
}
