#version 330

uniform mat4 matModel;
uniform mat4 matViewProjection;
uniform vec3 _SunDir;
uniform vec4 _Rotation;

layout(location = 0) in vec3 vertexPosition;
out float sunAmount;
out vec3 worldPosition;
out vec3 surfaceNormal;

vec3 RotateByQuaternion(vec3 value, vec4 rotation)
{
    return value + 2.0 * cross(rotation.xyz, cross(rotation.xyz, value) + rotation.w * value);
}

void main()
{
    // The official program forwards the object space unit direction, not a world normal: the height
    // cubemap is sampled in the planet's own frame, so the surface turns with the mesh transform
    // while `_Rotation` only carries the normal into world space for the sun term.
    vec3 planetDirection = normalize(vertexPosition);
    sunAmount = dot(RotateByQuaternion(planetDirection, _Rotation), _SunDir);
    vec4 world = matModel * vec4(vertexPosition, 1.0);
    worldPosition = world.xyz;
    surfaceNormal = planetDirection;
    gl_Position = matViewProjection * world;
}
