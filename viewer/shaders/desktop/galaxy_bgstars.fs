#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 _NoiseMapTexelSize;
uniform vec4 _StarSetting;
uniform vec4 _RefValueA;
uniform vec4 _RefValueB;
uniform vec4 _StarColor;
uniform vec4 _StarColorChange;
uniform vec4 _Time;
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
    // One star per noise texel. The source program mixes the two axes when it
    // derives the per-cell jitter, so the pairing below is kept literal.
    float scaledY = fragTexCoord.y / _NoiseMapTexelSize.y;
    float scaledX = fragTexCoord.x / _NoiseMapTexelSize.x;
    float cellY = trunc(scaledY) * _NoiseMapTexelSize.y;
    float cellX = trunc(scaledX) * _NoiseMapTexelSize.x;
    vec4 noise = texture(texture0, fragTexCoord);
    vec2 jitter = fract(vec2(cellY, cellX) * 37.0 + noise.xy);
    vec2 local = fract(vec2(scaledX, scaledY));

    float radius = noise.w * (_StarSetting.y - _StarSetting.x) + _StarSetting.x;
    vec2 center = jitter * (1.0 - 2.0 * radius) + radius;
    float core = clamp((radius - length(local - center)) / radius, 0.0, 1.0) * _StarSetting.z;
    core = core * core;
    core = core * core;
    if (core * core < 0.01) discard;

    float phase = sin(_Time.y * 1.5) * 0.5 + 0.5;
    vec3 reference = mix(_RefValueA.rgb, _RefValueB.rgb, phase);
    vec3 offset = noise.rgb - reference;
    float distance = abs(offset.r) + abs(offset.g) + abs(offset.b);
    float band = clamp((_StarSetting.w - distance * 0.5) / _StarSetting.w, 0.0, 1.0) * _StarSetting.z;
    if (band * band < 0.01) discard;

    vec3 outputLinear = (noise.rgb - _StarColor.rgb) * _StarColorChange.rgb + _StarColor.rgb;
    finalColor = vec4(_SceneLinear > 0.5 ? outputLinear : LinearToSrgb(outputLinear),
                      band * band * core * core);
}
