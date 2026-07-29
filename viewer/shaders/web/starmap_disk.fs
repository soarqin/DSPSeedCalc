#version 300 es
precision highp float;
precision highp int;

in highp vec2 fragTexCoord;

uniform highp sampler2D texture0;
uniform highp vec4 _TintColor;
uniform highp float _Multiplier;
uniform highp float _SceneLinear;

out highp vec4 finalColor;

highp vec3 LinearToSrgb(highp vec3 value)
{
    value = max(value, vec3(0.0));
    highp vec3 low = value * 12.92;
    highp vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

highp float SrgbToLinear(highp float value)
{
    return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}

void main()
{
    // `orbit-disk` is greyscale with R = G = B and alpha fixed at 1, so the viewer stores the red
    // channel alone, byte for byte as the source. Unity imports it as a colour texture and
    // linearizes on sample, which is what SrgbToLinear stands in for here.
    highp float pattern = SrgbToLinear(texture(texture0, fragTexCoord).r);
    // `YC Shaders/Particle/Add`: tint times multiplier, alpha saturated on its own, both times the
    // vertex colour (white for this quad) and the texture, and the product saturated.
    highp vec3 outputLinear = clamp(_TintColor.rgb * _Multiplier * pattern, 0.0, 1.0);
    highp float alpha = clamp(clamp(_TintColor.a, 0.0, 1.0), 0.0, 1.0);
    finalColor = vec4(_SceneLinear > 0.5 ? outputLinear : LinearToSrgb(outputLinear), alpha);
}
