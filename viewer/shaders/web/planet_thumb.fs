#version 300 es
precision highp float;
precision highp int;

uniform highp vec4 _Color;
uniform highp vec4 _RimColor;
uniform highp vec3 _WorldSpaceCameraPos;

in highp float sunAmount;
in highp vec3 worldPosition;
in highp vec3 worldNormal;
layout(location = 0) out highp vec4 finalColor;

void main()
{
    highp vec3 surface = _Color.rgb;
    highp vec3 viewDirection = normalize(_WorldSpaceCameraPos - worldPosition);
    highp float light = clamp(sunAmount * 4.0 + 0.5, 0.0, 1.0);
    highp float rim = pow(clamp(1.0 - dot(normalize(worldNormal), viewDirection), 0.0, 1.0), 2.5);
    highp vec3 color = surface * mix(0.16, 1.0, light);
    color += _RimColor.rgb * rim * _RimColor.a * 0.55;
    finalColor = vec4(color, 1.0);
}
