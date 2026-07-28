#version 330

uniform mat4 matModel;
uniform mat4 matViewProjection;
uniform vec3 _SunDir;
uniform vec4 _Rotation;

layout(location = 0) in vec3 vertexPosition;
out float sunAmount;
out vec3 worldPosition;
out vec3 worldNormal;

vec3 RotateByQuaternion(vec3 value, vec4 rotation)
{
    return value + 2.0 * cross(rotation.xyz, cross(rotation.xyz, value) + rotation.w * value);
}

void main()
{
    vec3 planetDirection = normalize(vertexPosition);
    sunAmount = dot(RotateByQuaternion(planetDirection, _Rotation), _SunDir);
    vec4 world = matModel * vec4(vertexPosition, 1.0);
    worldPosition = world.xyz;
    worldNormal = normalize(mat3(matModel) * vertexPosition);
    gl_Position = matViewProjection * world;
}
