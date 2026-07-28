#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec4 _TintColor;
uniform float _Multiplier;
uniform float _AlphaMultiplier;
uniform float _SceneLinear;

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
    // The source ring texture keeps RGB at 1 and carries the ring in alpha, so the
    // viewer stores that alpha alone in a single channel texture.
    float pattern = texture(texture0, fragTexCoord).r;
    vec3 outputLinear = _TintColor.rgb * _Multiplier;
    float alpha = clamp(pattern * _TintColor.a * _AlphaMultiplier, 0.0, 1.0);
    finalColor = vec4(_SceneLinear > 0.5 ? outputLinear : LinearToSrgb(outputLinear), alpha);
}
