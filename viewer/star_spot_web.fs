#version 300 es
precision mediump float;
precision highp int;

uniform highp vec4 _Multiplier;
uniform highp vec3 _SunGlowColor_xyz;
uniform highp vec4 _SunGlowPower;
uniform highp vec3 _SunCenterColor_xyz;
uniform highp vec4 _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower;
uniform highp vec4 _Global_WhiteMode0;

in highp vec2 varying_TEXCOORD0;
layout(location = 0) out highp vec4 out_SV_Target0;

highp float _37(highp vec2 _39, highp vec2 _40)
{
    return _39.y * _40.y + (_39.x * _40.x);
}
void main()
{
    highp vec2 _59 = vec2(varying_TEXCOORD0.x - 0.5, varying_TEXCOORD0.y - 0.5);
    highp float _62 = -sqrt(_37(_59, _59));
    highp float _63 = _62 * 2.0 + 1.0;
    highp float _71 = (_62 * 2.0 + _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower.y) / _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower.z;
    highp float _135 = isnan(0.0) ? _71 : (isnan(_71) ? 0.0 : max(_71, 0.0));
    highp float _77 = exp2(log2(isnan(1.0) ? _135 : (isnan(_135) ? 1.0 : min(_135, 1.0))) * _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower.w);
    if (_63 < 0.0)
    {
        discard;
    }
    highp float _106 = _63 / ((1.0 - _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower.y) + _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower.z);
    highp float _146 = isnan(0.0) ? _106 : (isnan(_106) ? 0.0 : max(_106, 0.0));
    highp float _112 = exp2(log2(isnan(1.0) ? _146 : (isnan(_146) ? 1.0 : min(_146, 1.0))) * _SunGlowPower.x);
    highp float _118 = (_112 * (_77 * (_SunCenterColor_xyz.x * _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower.x + (-_SunGlowColor_xyz.x)) + _SunGlowColor_xyz.x)) * _Multiplier.x;
    highp float _119 = (_112 * (_77 * (_SunCenterColor_xyz.y * _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower.x + (-_SunGlowColor_xyz.y)) + _SunGlowColor_xyz.y)) * _Multiplier.x;
    highp float _120 = (_112 * (_77 * (_SunCenterColor_xyz.z * _SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower.x + (-_SunGlowColor_xyz.z)) + _SunGlowColor_xyz.z)) * _Multiplier.x;
    bool _126 = _Global_WhiteMode0.x > 0.0;
    out_SV_Target0.x = _126 ? (_118 * 0.300000011920928955078125) : _118;
    out_SV_Target0.y = _126 ? (_119 * 0.300000011920928955078125) : _119;
    out_SV_Target0.z = _126 ? (_120 * 0.300000011920928955078125) : _120;
    out_SV_Target0.w = 1.0;
}
