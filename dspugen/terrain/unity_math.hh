#pragma once

#include <algorithm>
#include <cmath>

namespace dspugen::terrain {

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    [[nodiscard]] float sqrMagnitude() const { return x * x + y * y; }
    [[nodiscard]] Vector2 normalized() const;
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] float sqrMagnitude() const { return x * x + y * y + z * z; }
    [[nodiscard]] float magnitude() const { return std::sqrt(sqrMagnitude()); }
    [[nodiscard]] Vector3 normalized() const;
    void normalize();
};

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    [[nodiscard]] float sqrMagnitude() const {
        return x * x + y * y + z * z + w * w;
    }
    [[nodiscard]] Quaternion normalized() const;
};

inline Vector2 operator+(const Vector2 &lhs, const Vector2 &rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

inline Vector2 operator-(const Vector2 &lhs, const Vector2 &rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

inline Vector2 operator-(const Vector2 &value) {
    return {-value.x, -value.y};
}

inline Vector2 operator*(const Vector2 &value, float scale) {
    return {value.x * scale, value.y * scale};
}

inline Vector3 operator+(const Vector3 &lhs, const Vector3 &rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

inline Vector3 operator-(const Vector3 &lhs, const Vector3 &rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

inline Vector3 operator*(const Vector3 &value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

inline Vector3 operator*(float scale, const Vector3 &value) {
    return value * scale;
}

inline Vector3 operator/(const Vector3 &value, float scale) {
    return {value.x / scale, value.y / scale, value.z / scale};
}

Quaternion operator*(const Quaternion &lhs, const Quaternion &rhs);
Vector3 operator*(const Quaternion &rotation, const Vector3 &value);

inline float dot(const Vector3 &lhs, const Vector3 &rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Vector3 cross(const Vector3 &lhs, const Vector3 &rhs) {
    return {lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x};
}

Vector3 slerp(const Vector3 &a, const Vector3 &b, float t);
Quaternion angleAxis(float degrees, Vector3 axis);
Quaternion fromToRotation(Vector3 from, Vector3 to);
Vector3 inverseRotate(const Quaternion &rotation, Vector3 value);

}
