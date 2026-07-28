#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;
layout(location = 3) in vec4 vertexColor;

uniform mat4 matModel;
uniform mat4 matViewProjection;
uniform vec4 colDiffuse;
uniform vec4 _MainTex_ST;

out vec4 fragColor;
out vec2 fragTexCoord;

void main()
{
    vec4 worldPosition = matModel * vec4(vertexPosition, 1.0);
    gl_Position = matViewProjection * worldPosition;
    fragColor = vec4(1.0);
    fragTexCoord = vertexTexCoord * _MainTex_ST.xy + _MainTex_ST.zw;
}
