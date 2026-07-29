#version 300 es
precision highp float;
precision highp int;

in highp vec2 fragTexCoord;
in highp vec3 fragOrbitCamera;
in highp vec3 fragOrbitRay;

uniform highp sampler2D texture0;
uniform highp vec4 _LineColorA;
uniform highp vec4 _LineColorB;
uniform highp float _Multiplier;
uniform highp float _TileScale;
uniform highp float _DistScale;
uniform highp float _DistPower;
uniform highp float _MinDistScale;
uniform highp float _MaxDistScale;
uniform highp float _Radius;
uniform highp float _SceneLinear;

out highp vec4 finalColor;

highp vec3 LinearToSrgb(highp vec3 value)
{
    value = max(value, vec3(0.0));
    highp vec3 low = value * 12.92;
    highp vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

// `offset` is the candidate hit's signed distance from the orbit circle, `rayLength` the distance
// along the view ray that reached it, and `angle` its own angle around the circle. The longitudinal
// colour ramp is `u` raised to the fifth power, so the line is dark for most of the loop and
// brightens sharply just behind the planet.
highp vec4 OrbitSample(highp float offset, highp float rayLength, highp float angle, bool valid)
{
    highp float distanceScale = clamp(pow(rayLength * _DistScale, _DistPower), _MinDistScale, _MaxDistScale);
    highp float profileDistance = abs(offset) / distanceScale * _TileScale;
    if (!valid || profileDistance > 0.5) return vec4(0.0);
    highp vec4 profile = texture(texture0, vec2(fragTexCoord.x, profileDistance + profileDistance));
    highp float ramp = fract(angle * 0.15915494);
    highp float weight = ramp * ramp;
    weight = weight * weight * ramp;
    return profile * mix(_LineColorA, _LineColorB, weight);
}

void main()
{
    highp vec3 direction = normalize(fragOrbitRay);
    highp vec3 origin = fragOrbitCamera;
    highp float along = dot(origin, direction);
    highp vec3 perpendicular = origin - direction * along;
    highp float perpendicularSquared = dot(perpendicular, perpendicular);
    highp float perpendicularLength = sqrt(perpendicularSquared);
    // The official program keeps only rays that pass within 1.01 radii of the centre and rejects
    // the degenerate ray that goes straight through it.
    if (_Radius * 1.01 < perpendicularLength) discard;
    if (0.0 >= perpendicularLength) discard;

    bool inside = perpendicularLength < _Radius * 0.9999;
    highp float halfChord = inside ? sqrt(_Radius * _Radius - perpendicularSquared) : 0.0;
    highp float farLength = inside ? max(halfChord - along, 0.0) : 0.0;
    highp float nearLength = inside ? max(-halfChord - along, 0.0) : 0.0;
    highp float planeLength = direction.y == 0.0 ? 0.0 : max(-origin.y / direction.y, 0.0);
    highp vec3 farPoint = origin + direction * farLength;
    highp vec3 nearPoint = origin + direction * nearLength;
    highp vec3 planePoint = origin + direction * planeLength;

    // Two candidates take the out-of-plane offset where the ray enters and leaves the sphere of
    // radius `_Radius`; the third takes the radial offset where the ray crosses the orbital plane.
    // The brightest of the three is the drawn line, so the tube reads correctly both edge on and
    // face on.
    highp vec4 accumulated = OrbitSample(farPoint.y, farLength, atan(farPoint.z, farPoint.x),
                                   farLength != 0.0);
    accumulated = max(accumulated, OrbitSample(nearPoint.y, nearLength,
                                               atan(nearPoint.z, nearPoint.x), nearLength != 0.0));
    accumulated = max(accumulated, OrbitSample(length(planePoint) - _Radius, planeLength,
                                               atan(planePoint.z, planePoint.x), planeLength != 0.0));
    accumulated *= _Multiplier;
    finalColor = vec4(_SceneLinear > 0.5 ? accumulated.rgb : LinearToSrgb(accumulated.rgb),
                      accumulated.a);
}
