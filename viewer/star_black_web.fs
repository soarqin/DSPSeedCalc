#version 300 es
precision mediump float;
precision highp int;

uniform highp vec4 _TintColor;

layout(location = 0) out highp vec4 out_SV_Target0;

void main()
{
    out_SV_Target0.x = _TintColor.x;
    out_SV_Target0.y = _TintColor.y;
    out_SV_Target0.z = _TintColor.z;
    out_SV_Target0.w = _TintColor.w;
}
