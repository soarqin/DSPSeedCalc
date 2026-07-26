#version 330

invariant gl_Position;

uniform mat4 matModel;

uniform mat4 matViewProjection;

layout(location = 0) in vec3 in_POSITION0;
layout(location = 1) in vec2 in_TEXCOORD0;
out vec2 varying_TEXCOORD0;

void main()
{
    float _45 = in_POSITION0.y * matModel[1].x;
    float _46 = in_POSITION0.y * matModel[1].y;
    float _47 = in_POSITION0.y * matModel[1].z;
    float _48 = in_POSITION0.y * matModel[1].w;
    float _57 = matModel[0].x * in_POSITION0.x + _45;
    float _59 = matModel[0].y * in_POSITION0.x + _46;
    float _60 = matModel[0].z * in_POSITION0.x + _47;
    float _61 = matModel[0].w * in_POSITION0.x + _48;
    float _70 = matModel[2].x * in_POSITION0.z + _57;
    float _71 = matModel[2].y * in_POSITION0.z + _59;
    float _72 = matModel[2].z * in_POSITION0.z + _60;
    float _73 = matModel[2].w * in_POSITION0.z + _61;
    float _80 = _70 + matModel[3].x;
    float _81 = _71 + matModel[3].y;
    float _82 = _72 + matModel[3].z;
    float _83 = _73 + matModel[3].w;
    float _91 = _81 * matViewProjection[1].x;
    float _92 = _81 * matViewProjection[1].y;
    float _93 = _81 * matViewProjection[1].z;
    float _94 = _81 * matViewProjection[1].w;
    float _101 = matViewProjection[0].x * _80 + _91;
    float _102 = matViewProjection[0].y * _80 + _92;
    float _103 = matViewProjection[0].z * _80 + _93;
    float _104 = matViewProjection[0].w * _80 + _94;
    float _111 = matViewProjection[2].x * _82 + _101;
    float _112 = matViewProjection[2].y * _82 + _102;
    float _113 = matViewProjection[2].z * _82 + _103;
    float _114 = matViewProjection[2].w * _82 + _104;
    float _121 = matViewProjection[3].x * _83 + _111;
    float _122 = matViewProjection[3].y * _83 + _112;
    float _123 = matViewProjection[3].z * _83 + _113;
    float _124 = matViewProjection[3].w * _83 + _114;
    gl_Position.x = _121;
    gl_Position.y = _122;
    gl_Position.z = _123;
    gl_Position.w = _124;
    varying_TEXCOORD0.x = in_TEXCOORD0.x;
    varying_TEXCOORD0.y = in_TEXCOORD0.y;
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
}
