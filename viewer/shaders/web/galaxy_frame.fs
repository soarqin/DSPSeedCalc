#version 300 es
precision highp float;
precision highp int;

in highp vec2 fragTexCoord;

uniform highp sampler2D texture0;
uniform highp vec4 _Color;
uniform highp float _Multiplier;
uniform highp float _SceneLinear;

out highp vec4 finalColor;

vec3 LinearToSrgb(vec3 value)
{
    value = max(value, vec3(0.0));
    vec3 low = value * 12.92;
    vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

void main()
{
    // The source frame texture keeps RGB at 1 and carries the outline in alpha, so the
    // viewer stores that alpha alone in a single channel texture.
    float pattern = texture(texture0, fragTexCoord).r;
    if (pattern < 0.01) discard;
    vec3 outputLinear = _Color.rgb * _Multiplier;
    finalColor = vec4(_SceneLinear > 0.5 ? outputLinear : LinearToSrgb(outputLinear),
                      pattern * _Color.a * _Multiplier);
}
