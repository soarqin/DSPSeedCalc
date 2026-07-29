#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;

uniform mat4 matViewProjection;
uniform mat4 matOrbitRotation;
uniform vec3 _WorldSpaceCameraPos;
uniform vec4 _Position;

out vec2 fragTexCoord;
out vec3 fragOrbitCamera;
out vec3 fragOrbitRay;

void main()
{
    // The official vertex program forwards two vectors into the orbit's own frame, where the
    // fragment program intersects them with the circle: the camera position relative to
    // `_Position`, and the camera-to-vertex ray. It rotates both by the inverse of `_Rotation`.
    // The viewer uploads that same rotation as a matrix, so its inverse is the transpose.
    mat3 inverseOrbit = transpose(mat3(matOrbitRotation));
    fragTexCoord = vertexTexCoord;
    fragOrbitCamera = inverseOrbit * (_WorldSpaceCameraPos - _Position.xyz);
    fragOrbitRay = inverseOrbit * (vertexPosition - _WorldSpaceCameraPos);
    gl_Position = matViewProjection * vec4(vertexPosition, 1.0);
}
