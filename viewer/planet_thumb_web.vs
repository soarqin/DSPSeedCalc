#version 300 es

uniform highp mat4 matModel;
uniform highp mat4 matViewProjection;
uniform highp vec3 _SunDir;
uniform highp vec4 _Rotation;

layout(location = 0) in highp vec3 vertexPosition;
out highp float sunAmount;
out highp vec3 worldPosition;
out highp vec3 worldNormal;

highp vec3 RotateByQuaternion(highp vec3 value, highp vec4 rotation)
{
    return value + 2.0 * cross(rotation.xyz, cross(rotation.xyz, value) + rotation.w * value);
}

void main()
{
    highp vec3 planetDirection = normalize(vertexPosition);
    sunAmount = dot(RotateByQuaternion(planetDirection, _Rotation), _SunDir);
    highp vec4 world = matModel * vec4(vertexPosition, 1.0);
    worldPosition = world.xyz;
    worldNormal = normalize(mat3(matModel) * vertexPosition);
    gl_Position = matViewProjection * world;
}
