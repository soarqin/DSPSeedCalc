#version 300 es

layout(location = 0) in highp vec3 vertexPosition;
layout(location = 1) in highp vec2 vertexTexCoord;

uniform highp mat4 matViewProjection;
uniform highp mat4 matOrbitRotation;
uniform highp vec3 _WorldSpaceCameraPos;
uniform highp vec4 _Position;

out highp vec2 fragTexCoord;
out highp vec3 fragOrbitCamera;
out highp vec3 fragOrbitRay;

void main()
{
    // The official vertex program forwards two vectors into the orbit's own frame, where the
    // fragment program intersects them with the circle: the camera position relative to
    // `_Position`, and the camera-to-vertex ray. It rotates both by the inverse of `_Rotation`.
    // The viewer uploads that same rotation as a matrix, so its inverse is the transpose.
    highp mat3 inverseOrbit = transpose(mat3(matOrbitRotation));
    fragTexCoord = vertexTexCoord;
    fragOrbitCamera = inverseOrbit * (_WorldSpaceCameraPos - _Position.xyz);
    fragOrbitRay = inverseOrbit * (vertexPosition - _WorldSpaceCameraPos);
    gl_Position = matViewProjection * vec4(vertexPosition, 1.0);
}
