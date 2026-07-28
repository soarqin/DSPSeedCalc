#version 300 es

invariant gl_Position;

uniform highp mat4 matModel;

uniform highp mat4 matViewProjection;

layout(location = 0) in vec3 in_POSITION0;
out vec4 varying_TEXCOORD0;

void main()
{
    float _41 = in_POSITION0.y * matModel[1].x;
    float _42 = in_POSITION0.y * matModel[1].y;
    float _43 = in_POSITION0.y * matModel[1].z;
    float _44 = in_POSITION0.y * matModel[1].w;
    float _53 = matModel[0].x * in_POSITION0.x + _41;
    float _55 = matModel[0].y * in_POSITION0.x + _42;
    float _56 = matModel[0].z * in_POSITION0.x + _43;
    float _57 = matModel[0].w * in_POSITION0.x + _44;
    float _66 = matModel[2].x * in_POSITION0.z + _53;
    float _67 = matModel[2].y * in_POSITION0.z + _55;
    float _68 = matModel[2].z * in_POSITION0.z + _56;
    float _69 = matModel[2].w * in_POSITION0.z + _57;
    float _76 = _66 + matModel[3].x;
    float _77 = _67 + matModel[3].y;
    float _78 = _68 + matModel[3].z;
    float _79 = _69 + matModel[3].w;
    float _87 = _77 * matViewProjection[1].x;
    float _88 = _77 * matViewProjection[1].y;
    float _89 = _77 * matViewProjection[1].z;
    float _90 = _77 * matViewProjection[1].w;
    float _97 = matViewProjection[0].x * _76 + _87;
    float _98 = matViewProjection[0].y * _76 + _88;
    float _99 = matViewProjection[0].z * _76 + _89;
    float _100 = matViewProjection[0].w * _76 + _90;
    float _107 = matViewProjection[2].x * _78 + _97;
    float _108 = matViewProjection[2].y * _78 + _98;
    float _109 = matViewProjection[2].z * _78 + _99;
    float _110 = matViewProjection[2].w * _78 + _100;
    float _117 = matViewProjection[3].x * _79 + _107;
    float _118 = matViewProjection[3].y * _79 + _108;
    float _119 = matViewProjection[3].z * _79 + _109;
    float _120 = matViewProjection[3].w * _79 + _110;
    gl_Position.x = _117;
    gl_Position.y = _118;
    gl_Position.z = _119;
    gl_Position.w = _120;
    varying_TEXCOORD0.x = _117;
    varying_TEXCOORD0.y = _118;
    varying_TEXCOORD0.z = _119;
    varying_TEXCOORD0.w = _120;
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
}
