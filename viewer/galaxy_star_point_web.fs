#version 300 es
precision mediump float;
precision highp int;

in lowp vec4 fragColor;
in highp vec2 fragTexCoord;

uniform sampler2D texture0;
uniform highp vec4 _TintColor;
uniform highp float _Multiplier;
uniform highp float _AlphaMultiplier;
uniform highp float _PointGlow;
uniform highp float _SceneLinear;
uniform highp float _Global_GalaxyFadeRadius;
uniform highp vec4 _ScreenParams;

out highp vec4 finalColor;

highp vec3 SrgbToLinear(highp vec3 value)
{
    highp vec3 low = value / 12.92;
    highp vec3 high = pow((value + 0.055) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), value));
}

highp vec3 LinearToSrgb(highp vec3 value)
{
    value = max(value, vec3(0.0));
    highp vec3 low = value * 12.92;
    highp vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

void main()
{
    highp vec2 centeredPixel = gl_FragCoord.xy - _ScreenParams.xy * 0.5;
    highp float edge = clamp((length(centeredPixel) - (_Global_GalaxyFadeRadius - 20.0)) / 20.0, 0.0, 1.0);
    highp float edgeFade = (edge * edge * (3.0 - 2.0 * edge)) * 0.9 + 0.1;
    highp vec4 sampleColor = texture(texture0, fragTexCoord);
    highp float alpha;
    highp vec3 rgb;
    alpha = clamp(sampleColor.a * _TintColor.a * _AlphaMultiplier, 0.0, 1.0);
    rgb = sampleColor.rgb * SrgbToLinear(_TintColor.rgb) * _Multiplier;
    highp vec3 outputLinear = edgeFade * rgb * fragColor.rgb;
    finalColor = vec4(_SceneLinear > 0.5 ? outputLinear : LinearToSrgb(outputLinear),
                      edgeFade * alpha * fragColor.a);
}
