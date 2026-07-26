#version 330

in vec4 fragColor;
in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec4 _TintColor;
uniform float _Multiplier;
uniform float _AlphaMultiplier;
uniform float _PointGlow;
uniform float _SceneLinear;
uniform float _Global_GalaxyFadeRadius;
uniform vec4 _ScreenParams;

out vec4 finalColor;

vec3 SrgbToLinear(vec3 value)
{
    vec3 low = value / 12.92;
    vec3 high = pow((value + 0.055) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), value));
}

vec3 LinearToSrgb(vec3 value)
{
    value = max(value, vec3(0.0));
    vec3 low = value * 12.92;
    vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

void main()
{
    vec2 centeredPixel = gl_FragCoord.xy - _ScreenParams.xy * 0.5;
    float edge = clamp((length(centeredPixel) - (_Global_GalaxyFadeRadius - 20.0)) / 20.0, 0.0, 1.0);
    float edgeFade = (edge * edge * (3.0 - 2.0 * edge)) * 0.9 + 0.1;
    vec4 sampleColor = texture(texture0, fragTexCoord);
    float alpha;
    vec3 rgb;
    alpha = clamp(sampleColor.a * _TintColor.a * _AlphaMultiplier, 0.0, 1.0);
    rgb = sampleColor.rgb * SrgbToLinear(_TintColor.rgb) * _Multiplier;
    vec3 outputLinear = edgeFade * rgb * fragColor.rgb;
    finalColor = vec4(_SceneLinear > 0.5 ? outputLinear : LinearToSrgb(outputLinear),
                      edgeFade * alpha * fragColor.a);
}
