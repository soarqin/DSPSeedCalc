#version 300 es
precision highp float;
precision highp int;

uniform highp samplerCube _HeightMap;
uniform highp vec4 _Color;
uniform highp vec4 _ShoreLineColor;
uniform highp vec4 _RimColor;
uniform highp vec4 _ColorBio0;
uniform highp vec4 _ColorBio1;
uniform highp vec4 _ColorBio2;
uniform highp vec4 _HeightSettings;
uniform highp vec4 _Rotation;
uniform highp float _ShoreInvThick;
uniform highp float _ShoreHeight;
uniform highp float _WireIntens;
uniform highp float _BioStrength;
uniform highp float _FarHeight;
uniform highp float _Diameter;
uniform highp vec3 _WorldSpaceCameraPos;
uniform highp float _SceneLinear;

in highp float sunAmount;
in highp vec3 worldPosition;
in highp vec3 surfaceNormal;
layout(location = 0) out highp vec4 finalColor;

highp vec3 LinearToSrgb(highp vec3 value)
{
    value = max(value, vec3(0.0));
    highp vec3 low = value * 12.92;
    highp vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

highp vec3 RotateByQuaternion(highp vec3 value, highp vec4 rotation)
{
    return value + 2.0 * cross(rotation.xyz, cross(rotation.xyz, value) + rotation.w * value);
}

void main()
{
    highp vec3 normal = normalize(surfaceNormal);
    highp float longitude = atan(normal.x, normal.z);
    highp float latitude = asin(normal.y);
    highp float latitudeCosine = cos(latitude);
    // The graticule cells are ten degrees wide both ways; 5.729578 is a tenth of one radian in
    // degrees. Meridians drop out toward the poles on the twenty degree beat of cos(18 * longitude),
    // which is what keeps them from crowding together at the pole.
    highp float longitudeCell = fract(longitude * 5.729578);
    highp float latitudeCell = fract(latitude * 5.729578);
    highp float meridianBeat = (cos(longitude * 18.0) + 1.0) * 0.5;
    highp float meridianMask = clamp((latitudeCosine - 0.45) * 20.0, 0.0, 1.0) * (1.0 - meridianBeat) +
                         meridianBeat;

    // `_Diameter` is the nominal starmap planet diameter, so `far` is roughly how many planet widths
    // away the camera sits. Everything below scales the graticule and the rim with it, which is why
    // the lines only resolve when the camera is almost touching the planet.
    highp float cameraDistance = length(worldPosition - _WorldSpaceCameraPos);
    highp float far = max(cameraDistance / _Diameter - 2.0, 0.0);
    highp float wireWidth = (1.0 + 0.6 * far) * 0.04;
    highp float meridianWidth = wireWidth / (latitudeCosine + 0.01);
    highp float meridian = clamp(max((meridianWidth - longitudeCell) / meridianWidth,
                               (longitudeCell - 1.0 + meridianWidth) / meridianWidth), 0.0, 1.0);
    meridian *= clamp(latitudeCosine * 10.0, 0.0, 1.0);
    highp float parallel = clamp(max((latitudeCell - 1.0 + wireWidth) / wireWidth,
                               (wireWidth - latitudeCell) / wireWidth), 0.0, 1.0);
    highp float wireFade = 1.0 / (1.0 + 0.3 * far);
    highp float wire = (meridian * meridianMask + parallel) * 0.7;
    wire = wire * wire * wireFade;
    highp float rimPower = wireFade * 2.5 + 2.5;
    highp float distanceGain = min(far * 0.007, 0.25) + 0.75;
    highp float shoreFade = min(1.0 / (1.0 + 0.3 * (far - 1.0)), 1.0);
    highp float farFade = min(1.0 / (1.0 + 0.1 * (far - 1.0)), 1.0);

    // No official thumb material assigns `_HeightMap`, and the shader property defaults to Unity's
    // built in black cubemap, so a planet the game has not generated terrain for samples zero here.
    // The viewer never generates terrain, so it binds a black cubemap and takes the same path.
    highp vec4 heightSample = texture(_HeightMap, normal);
    highp float heightRange = _HeightSettings.y - _HeightSettings.x;
    highp float heightScale = _HeightSettings.w - _HeightSettings.z;
    highp float height = clamp((heightSample.x - _HeightSettings.x) / heightRange, 0.0, 1.0) * heightScale +
                   _HeightSettings.z;
    highp float farHeight = _FarHeight * heightScale + _HeightSettings.z;
    height = farFade * (height - farHeight) + farHeight;
    highp float surfaceAmount = (wire * _WireIntens + 1.0) * height + wire * _WireIntens * 0.2;

    highp vec3 viewDirection = normalize(_WorldSpaceCameraPos - worldPosition);
    highp vec3 worldNormal = RotateByQuaternion(normal, _Rotation);
    highp float viewAmount = clamp(dot(worldNormal, viewDirection), 0.0, 1.0);
    highp float silhouette = min(viewAmount * 10.0, 1.0);
    highp float rim = pow(1.0 - viewAmount, rimPower);
    highp float sunFront = clamp(sunAmount * 4.0 + 0.5, 0.0, 1.0);
    highp float sunWrap = clamp(sunAmount * 0.5 + 0.5, 0.0, 1.0);
    highp float light = silhouette * sunFront;
    light = light * light * (3.0 - 2.0 * light);
    light = (light * 0.8 + 0.2) * distanceGain;
    highp float base = max(surfaceAmount * light + light * 0.5, 0.0);

    highp float shore = clamp(1.0 - abs(heightSample.x - _ShoreHeight) * _ShoreInvThick * shoreFade,
                        0.0, 1.0);
    shore = shore * (sunWrap * 0.4 + 0.6) * light * shoreFade;
    highp vec4 accumulated = base * _Color + shore * _ShoreLineColor;

    highp float bandFraction = fract(heightSample.y);
    highp float band = bandFraction * bandFraction * (3.0 - 2.0 * bandFraction) +
                 (heightSample.y - bandFraction);
    highp vec4 bio = _ColorBio1 * min(clamp(band, 0.0, 1.0), clamp(2.0 - band, 0.0, 1.0)) +
               _ColorBio0 * clamp(1.0 - band, 0.0, 1.0) +
               _ColorBio2 * clamp(band - 1.0, 0.0, 1.0);
    bio = bio * _BioStrength * light;
    accumulated += _BioStrength > 1.0e-5 ? bio : vec4(0.0);

    accumulated += (clamp(sunAmount + 0.5, 0.0, 1.0) * 0.5 * rim * farFade * 1.15) * _RimColor;
    highp vec3 color = silhouette * (accumulated.a * accumulated.rgb);
    finalColor = vec4(_SceneLinear > 0.5 ? color : LinearToSrgb(color), 1.0);
}
