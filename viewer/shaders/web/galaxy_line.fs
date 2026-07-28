#version 300 es
precision highp float;
precision highp int;

in highp vec2 fragTexCoord;
in highp float fragCameraDistance;

uniform highp sampler2D texture0;
uniform highp vec4 _LineColorA;
uniform highp vec4 _LineColorB;
uniform highp float _Multiplier;
uniform highp float _TileScale;
uniform highp float _DistScale;
uniform highp float _DistPower;
uniform highp float _MinDistScale;
uniform highp float _MaxDistScale;
uniform highp float _Global_GalaxyFadeRadius;
uniform highp float _SceneLinear;
uniform highp vec4 _ScreenParams;

out highp vec4 finalColor;

highp vec3 LinearToSrgb(highp vec3 value)
{
    value = max(value, vec3(0.0));
    highp vec3 low = value * 12.92;
    highp vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

void main()
{
    highp float distanceScale = clamp(pow(fragCameraDistance * _DistScale, _DistPower),
                                      _MinDistScale, _MaxDistScale);
    highp float profileDistance = abs(fragTexCoord.y - 0.5) / distanceScale;
    highp float profileCoord = profileDistance * _TileScale + profileDistance * _TileScale;
    if (profileCoord > 1.0) discard;

    highp vec4 profile = texture(texture0, vec2(fragTexCoord.x, profileCoord));
    highp vec3 lineRgb = mix(_LineColorA.rgb, _LineColorB.rgb, fragTexCoord.x);
    highp float lineAlpha = mix(_LineColorA.a, _LineColorB.a, fragTexCoord.x);
    highp vec2 centeredPixel = gl_FragCoord.xy - _ScreenParams.xy * 0.5;
    highp float edge = clamp((length(centeredPixel) - (_Global_GalaxyFadeRadius - 20.0)) / 20.0, 0.0, 1.0);
    highp float edgeFade = (edge * edge * (3.0 - 2.0 * edge)) * 0.5 + 0.5;
    highp vec3 outputLinear = profile.rgb * lineRgb * _Multiplier * edgeFade;
    finalColor = vec4(_SceneLinear > 0.5 ? outputLinear : LinearToSrgb(outputLinear),
                      profile.a * lineAlpha * _Multiplier * edgeFade);
}
