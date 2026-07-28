#version 300 es
precision highp float;
precision highp int;

layout(location = 0) in highp vec3 vertexPosition;

uniform highp mat4 matModel;
uniform highp mat4 matViewProjection;

out highp vec2 fragWorldXZ;

void main()
{
    vec4 worldPosition = matModel * vec4(vertexPosition, 1.0);
    fragWorldXZ = worldPosition.xz;
    gl_Position = matViewProjection * worldPosition;
}
