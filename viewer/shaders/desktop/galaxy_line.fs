#version 330

in vec2 fragTexCoord;
in float fragCameraDistance;

uniform sampler2D texture0;
uniform vec4 _LineColorA;
uniform vec4 _LineColorB;
uniform float _Multiplier;
uniform float _TileScale;
uniform float _DistScale;
uniform float _DistPower;
uniform float _MinDistScale;
uniform float _MaxDistScale;
uniform float _Global_GalaxyFadeRadius;
uniform float _SceneLinear;
uniform vec4 _ScreenParams;

out vec4 finalColor;

vec3 LinearToSrgb(vec3 value)
{
    value = max(value, vec3(0.0));
    vec3 low = value * 12.92;
    vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

void main()
{
    float distanceScale = clamp(pow(fragCameraDistance * _DistScale, _DistPower),
                                _MinDistScale, _MaxDistScale);
    float profileDistance = abs(fragTexCoord.y - 0.5) / distanceScale;
    float profileCoord = profileDistance * _TileScale + profileDistance * _TileScale;
    if (profileCoord > 1.0) discard;

    vec4 profile = texture(texture0, vec2(fragTexCoord.x, profileCoord));
    vec3 lineRgb = mix(_LineColorA.rgb, _LineColorB.rgb, fragTexCoord.x);
    float lineAlpha = mix(_LineColorA.a, _LineColorB.a, fragTexCoord.x);
    vec2 centeredPixel = gl_FragCoord.xy - _ScreenParams.xy * 0.5;
    float edge = clamp((length(centeredPixel) - (_Global_GalaxyFadeRadius - 20.0)) / 20.0, 0.0, 1.0);
    float edgeFade = (edge * edge * (3.0 - 2.0 * edge)) * 0.5 + 0.5;
    vec3 outputLinear = profile.rgb * lineRgb * _Multiplier * edgeFade;
    finalColor = vec4(_SceneLinear > 0.5 ? outputLinear : LinearToSrgb(outputLinear),
                      profile.a * lineAlpha * _Multiplier * edgeFade);
}
