#include "unity_math.hh"

namespace dspugen::terrain {

namespace {

constexpr float Epsilon = 1.0E-05f;
constexpr float DegToHalfRad = 0.0087266462599716478846f;

Vector3 lerp(const Vector3 &a, const Vector3 &b, float t) {
    return a + (b - a) * t;
}

Vector3 orthonormalVector(const Vector3 &value) {
    // Unity's anti-parallel Slerp branch chooses a stable perpendicular axis.
    if (std::abs(value.z) < 0.9999999f)
        return Vector3{-value.y, value.x, 0.0f}.normalized();
    return Vector3{0.0f, -value.z, value.y}.normalized();
}

Vector3 rotateAroundAxis(const Vector3 &value, const Vector3 &axis, float angle) {
    auto cosine = std::cos(angle);
    auto sine = std::sin(angle);
    return value * cosine + cross(axis, value) * sine +
           axis * (dot(axis, value) * (1.0f - cosine));
}

}

Vector2 Vector2::normalized() const {
    auto length = static_cast<float>(std::sqrt(static_cast<double>(sqrMagnitude())));
    if (!(length > Epsilon)) return {0.0f, 0.0f};
    return {x / length, y / length};
}

Vector3 Vector3::normalized() const {
    auto length = magnitude();
    if (!(length > Epsilon)) return {0.0f, 0.0f, 0.0f};
    return *this / length;
}

void Vector3::normalize() {
    *this = normalized();
}

Quaternion Quaternion::normalized() const {
    auto length = static_cast<float>(std::sqrt(static_cast<double>(sqrMagnitude())));
    if (!(length > Epsilon)) return {};
    return {x / length, y / length, z / length, w / length};
}

Quaternion operator*(const Quaternion &lhs, const Quaternion &rhs) {
    return {
        lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y + lhs.w * rhs.x,
        (0.0f - lhs.x) * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x + lhs.w * rhs.y,
        lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w + lhs.w * rhs.z,
        (0.0f - lhs.x) * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z + lhs.w * rhs.w,
    };
}

Vector3 operator*(const Quaternion &rotation, const Vector3 &value) {
    Vector3 doubled{value.x * 2.0f, value.y * 2.0f, value.z * 2.0f};
    auto num = rotation.w * rotation.w - 0.5f;
    auto num2 = rotation.x * doubled.x + rotation.y * doubled.y +
                rotation.z * doubled.z;
    return {
        doubled.x * num +
            (rotation.y * doubled.z - rotation.z * doubled.y) * rotation.w +
            rotation.x * num2,
        doubled.y * num +
            (rotation.z * doubled.x - rotation.x * doubled.z) * rotation.w +
            rotation.y * num2,
        doubled.z * num +
            (rotation.x * doubled.y - rotation.y * doubled.x) * rotation.w +
            rotation.z * num2,
    };
}

Vector3 inverseRotate(const Quaternion &rotation, Vector3 value) {
    value.x *= 2.0f;
    value.y *= 2.0f;
    value.z *= 2.0f;
    auto num = rotation.w * rotation.w - 0.5f;
    auto num2 = rotation.x * value.x + rotation.y * value.y +
                rotation.z * value.z;
    return {
        value.x * num -
            (rotation.y * value.z - rotation.z * value.y) * rotation.w +
            rotation.x * num2,
        value.y * num -
            (rotation.z * value.x - rotation.x * value.z) * rotation.w +
            rotation.y * num2,
        value.z * num -
            (rotation.x * value.y - rotation.y * value.x) * rotation.w +
            rotation.z * num2,
    };
}

Quaternion angleAxis(float degrees, Vector3 axis) {
    axis.normalize();
    if (!(axis.sqrMagnitude() > Epsilon * Epsilon)) return {};
    auto halfRadians = degrees * DegToHalfRad;
    auto sine = static_cast<float>(std::sin(static_cast<double>(halfRadians)));
    auto cosine = static_cast<float>(std::cos(static_cast<double>(halfRadians)));
    return {axis.x * sine, axis.y * sine, axis.z * sine, cosine};
}

Quaternion fromToRotation(Vector3 from, Vector3 to) {
    from.normalize();
    to.normalize();
    auto cosine = std::clamp(dot(from, to), -1.0f, 1.0f);
    if (cosine > 1.0f - Epsilon) return {};
    if (cosine < -1.0f + Epsilon) {
        auto axis = cross(from, {1.0f, 0.0f, 0.0f});
        if (!(axis.sqrMagnitude() > Epsilon * Epsilon))
            axis = cross(from, {0.0f, 0.0f, 1.0f});
        return angleAxis(180.0f, axis);
    }
    auto axis = cross(from, to);
    return Quaternion{axis.x, axis.y, axis.z, 1.0f + cosine}.normalized();
}

Vector3 slerp(const Vector3 &a, const Vector3 &b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    auto magnitudeA = a.magnitude();
    auto magnitudeB = b.magnitude();
    if (!(magnitudeA > Epsilon) || !(magnitudeB > Epsilon))
        return lerp(a, b, t);

    auto normalA = a / magnitudeA;
    auto normalB = b / magnitudeB;
    auto dotValue = std::clamp(dot(normalA, normalB), -1.0f, 1.0f);
    auto magnitude = magnitudeA + (magnitudeB - magnitudeA) * t;

    if (dotValue > 1.0f - Epsilon)
        return lerp(a, b, t);

    if (dotValue < -1.0f + Epsilon) {
        auto axis = orthonormalVector(normalA);
        return rotateAroundAxis(normalA, axis, 3.14159265358979323846f * t) * magnitude;
    }

    auto axis = cross(a, b).normalized();
    auto angle = std::acos(dotValue) * t;
    return rotateAroundAxis(normalA, axis, angle) * magnitude;
}

}
