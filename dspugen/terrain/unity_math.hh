#pragma once

#include <algorithm>
#include <cmath>

namespace dspugen::terrain {

// Mathf.PI, spelled as the widened value of the single-precision constant so it
// is obvious this is a float and not a truncation of Math.PI.
inline constexpr float MathfPi = 3.14159274101257324219f;

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    // Mono evaluates float32 expressions in double and rounds when the float32
    // result is produced, so these sums round once, not once per operation.
    [[nodiscard]] float sqrMagnitude() const {
        return static_cast<float>(static_cast<double>(x) * static_cast<double>(x) +
                                  static_cast<double>(y) * static_cast<double>(y));
    }
    [[nodiscard]] Vector2 normalized() const;
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] double sqrMagnitudeDouble() const {
        return static_cast<double>(x) * static_cast<double>(x) +
               static_cast<double>(y) * static_cast<double>(y) +
               static_cast<double>(z) * static_cast<double>(z);
    }
    [[nodiscard]] float sqrMagnitude() const {
        return static_cast<float>(sqrMagnitudeDouble());
    }
    // Vector3.Magnitude is (float)Math.Sqrt(x*x + y*y + z*z): the sum stays in
    // double and only the square root's result is narrowed.
    [[nodiscard]] float magnitude() const {
        return static_cast<float>(std::sqrt(sqrMagnitudeDouble()));
    }
    [[nodiscard]] Vector3 normalized() const;
    void normalize();
};

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    [[nodiscard]] float sqrMagnitude() const {
        return static_cast<float>(static_cast<double>(x) * static_cast<double>(x) +
                                  static_cast<double>(y) * static_cast<double>(y) +
                                  static_cast<double>(z) * static_cast<double>(z) +
                                  static_cast<double>(w) * static_cast<double>(w));
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

// UnityEngine.Quaternion.operator* (managed, UnityEngine.CoreModule).
Quaternion operator*(const Quaternion &lhs, const Quaternion &rhs);

// Two different rotate-a-vector routines exist in the reference and they sum
// their terms in different orders, so they are not interchangeable at float
// precision.  Pick by call site, not by convenience:
//   unityRotate  - UnityEngine.Quaternion.operator*(Quaternion, Vector3),
//                  used by PlanetAlgorithm.GenerateVeins for the node basis.
//   qRotate      - the game's Maths.QRotate, used by PlanetData.PredictPose.
Vector3 unityRotate(const Quaternion &rotation, const Vector3 &point);
Vector3 qRotate(const Quaternion &rotation, Vector3 value);

// Vector3.Dot returns float32, so the three products and two sums are all
// evaluated in double and the result is narrowed once, at the return.
inline float dot(const Vector3 &lhs, const Vector3 &rhs) {
    return static_cast<float>(static_cast<double>(lhs.x) * static_cast<double>(rhs.x) +
                              static_cast<double>(lhs.y) * static_cast<double>(rhs.y) +
                              static_cast<double>(lhs.z) * static_cast<double>(rhs.z));
}

// Vector3.Cross passes each component straight into the Vector3 constructor, so
// every component is evaluated in double and narrowed once, at that float32
// argument -- not once per multiply and subtract.
inline Vector3 cross(const Vector3 &lhs, const Vector3 &rhs) {
    return {static_cast<float>(static_cast<double>(lhs.y) * static_cast<double>(rhs.z) -
                               static_cast<double>(lhs.z) * static_cast<double>(rhs.y)),
            static_cast<float>(static_cast<double>(lhs.z) * static_cast<double>(rhs.x) -
                               static_cast<double>(lhs.x) * static_cast<double>(rhs.z)),
            static_cast<float>(static_cast<double>(lhs.x) * static_cast<double>(rhs.y) -
                               static_cast<double>(lhs.y) * static_cast<double>(rhs.x))};
}

Vector3 slerp(const Vector3 &a, const Vector3 &b, float t);
Quaternion angleAxis(float degrees, Vector3 axis);
Quaternion fromToRotation(Vector3 from, Vector3 to);
Vector3 inverseRotate(const Quaternion &rotation, Vector3 value);

}
