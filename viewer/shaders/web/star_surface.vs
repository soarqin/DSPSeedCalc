#version 300 es

invariant gl_Position;

uniform highp mat4 matModel;

uniform highp mat4 matViewProjection;

layout(location = 0) in vec4 in_POSITION0;
layout(location = 2) in vec3 in_NORMAL0;
out vec3 varying_TEXCOORD0;
out vec3 varying_TEXCOORD1;
out vec3 varying_TEXCOORD2;

float _32(vec3 _34, vec3 _35)
{
    return _34.z * _35.z + (_34.y * _35.y + (_34.x * _35.x));
}
void main()
{
    float _61 = in_POSITION0.y * matModel[1].x;
    float _62 = in_POSITION0.y * matModel[1].y;
    float _63 = in_POSITION0.y * matModel[1].z;
    float _64 = in_POSITION0.y * matModel[1].w;
    float _73 = matModel[0].x * in_POSITION0.x + _61;
    float _74 = matModel[0].y * in_POSITION0.x + _62;
    float _75 = matModel[0].z * in_POSITION0.x + _63;
    float _76 = matModel[0].w * in_POSITION0.x + _64;
    float _85 = matModel[2].x * in_POSITION0.z + _73;
    float _86 = matModel[2].y * in_POSITION0.z + _74;
    float _87 = matModel[2].z * in_POSITION0.z + _75;
    float _88 = matModel[2].w * in_POSITION0.z + _76;
    float _95 = _85 + matModel[3].x;
    float _96 = _86 + matModel[3].y;
    float _97 = _87 + matModel[3].z;
    float _98 = _88 + matModel[3].w;
    varying_TEXCOORD1.x = matModel[3].x * in_POSITION0.w + _85;
    varying_TEXCOORD1.y = matModel[3].y * in_POSITION0.w + _86;
    varying_TEXCOORD1.z = matModel[3].z * in_POSITION0.w + _87;
    float _120 = _96 * matViewProjection[1].x;
    float _121 = _96 * matViewProjection[1].y;
    float _122 = _96 * matViewProjection[1].z;
    float _123 = _96 * matViewProjection[1].w;
    float _130 = matViewProjection[0].x * _95 + _120;
    float _131 = matViewProjection[0].y * _95 + _121;
    float _132 = matViewProjection[0].z * _95 + _122;
    float _133 = matViewProjection[0].w * _95 + _123;
    float _140 = matViewProjection[2].x * _97 + _130;
    float _141 = matViewProjection[2].y * _97 + _131;
    float _142 = matViewProjection[2].z * _97 + _132;
    float _143 = matViewProjection[2].w * _97 + _133;
    float _150 = matViewProjection[3].x * _98 + _140;
    float _151 = matViewProjection[3].y * _98 + _141;
    float _152 = matViewProjection[3].z * _98 + _142;
    float _153 = matViewProjection[3].w * _98 + _143;
    gl_Position.x = _150;
    gl_Position.y = _151;
    gl_Position.z = _152;
    gl_Position.w = _153;
    vec3 _164 = vec3(in_POSITION0.x, in_POSITION0.y, in_POSITION0.z);
    float _166 = inversesqrt(_32(_164, _164));
    varying_TEXCOORD0.x = _166 * in_POSITION0.x;
    varying_TEXCOORD0.y = _166 * in_POSITION0.y;
    varying_TEXCOORD0.z = _166 * in_POSITION0.z;
    varying_TEXCOORD2.x = matModel[2].x * in_NORMAL0.z + (matModel[0].x * in_NORMAL0.x + (in_NORMAL0.y * matModel[1].x));
    varying_TEXCOORD2.y = matModel[2].y * in_NORMAL0.z + (matModel[0].y * in_NORMAL0.x + (in_NORMAL0.y * matModel[1].y));
    varying_TEXCOORD2.z = matModel[2].z * in_NORMAL0.z + (matModel[0].z * in_NORMAL0.x + (in_NORMAL0.y * matModel[1].z));
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
}
