#version 330

uniform vec4 _Color;
uniform vec4 _RimColor;
uniform vec3 _WorldSpaceCameraPos;

in float sunAmount;
in vec3 worldPosition;
in vec3 worldNormal;
layout(location = 0) out vec4 finalColor;

void main()
{
    vec3 surface = _Color.rgb;
    vec3 viewDirection = normalize(_WorldSpaceCameraPos - worldPosition);
    float light = clamp(sunAmount * 4.0 + 0.5, 0.0, 1.0);
    float rim = pow(clamp(1.0 - dot(normalize(worldNormal), viewDirection), 0.0, 1.0), 2.5);
    vec3 color = surface * mix(0.16, 1.0, light);
    color += _RimColor.rgb * rim * _RimColor.a * 0.55;
    finalColor = vec4(color, 1.0);
}
