#include "unity_math.hh"

#include "ucrt_trig.hh"

namespace dspugen::terrain {

namespace {

constexpr float Epsilon = 1.0E-05f;

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

// Vector2.Normalize divides by Vector2.magnitude, which is
// (float)Math.Sqrt(x * x + y * y) with the sum written inline, so the sum stays
// in double and only the square root's result is narrowed.  Going through
// sqrMagnitude would narrow the sum first and round twice -- that is the shape
// Quaternion.Normalize has (Mathf.Sqrt(Dot(q, q)), where Dot returns float32),
// but not this one.
Vector2 Vector2::normalized() const {
    const auto length = static_cast<float>(std::sqrt(
        static_cast<double>(x) * static_cast<double>(x) +
        static_cast<double>(y) * static_cast<double>(y)));
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

// Term order is copied from UnityEngine.Quaternion.operator*, which is managed
// code.  The game's Maths.QMultiply computes the same product but sums the four
// terms in a different order; float addition is not associative, so the two
// disagree in the last bit and must not be substituted for one another.  Every
// quaternion product on the vein path (PlanetGen.runtimeOrbitRotation,
// runtimeSystemRotation, PlanetData.PredictPose) uses the Unity operator.
Quaternion operator*(const Quaternion &lhs, const Quaternion &rhs) {
    // Each component is a float32 constructor argument, so the four products
    // and three sums inside it are evaluated in double and rounded once.
    const auto lx = static_cast<double>(lhs.x);
    const auto ly = static_cast<double>(lhs.y);
    const auto lz = static_cast<double>(lhs.z);
    const auto lw = static_cast<double>(lhs.w);
    const auto rx = static_cast<double>(rhs.x);
    const auto ry = static_cast<double>(rhs.y);
    const auto rz = static_cast<double>(rhs.z);
    const auto rw = static_cast<double>(rhs.w);
    return {
        static_cast<float>(lw * rx + lx * rw + ly * rz - lz * ry),
        static_cast<float>(lw * ry + ly * rw + lz * rx - lx * rz),
        static_cast<float>(lw * rz + lz * rw + lx * ry - ly * rx),
        static_cast<float>(lw * rw - lx * rx - ly * ry - lz * rz),
    };
}

Vector3 unityRotate(const Quaternion &rotation, const Vector3 &point) {
    auto num = rotation.x * 2.0f;
    auto num2 = rotation.y * 2.0f;
    auto num3 = rotation.z * 2.0f;
    auto num4 = rotation.x * num;
    auto num5 = rotation.y * num2;
    auto num6 = rotation.z * num3;
    auto num7 = rotation.x * num2;
    auto num8 = rotation.x * num3;
    auto num9 = rotation.y * num3;
    auto num10 = rotation.w * num;
    auto num11 = rotation.w * num2;
    auto num12 = rotation.w * num3;
    // num..num12 are float locals in the reference and each is a single
    // multiply, so they round the same way here.  The three result expressions
    // are assigned to float32 fields, so each of those rounds only once.
    const auto d = [](float value) { return static_cast<double>(value); };
    return {
        static_cast<float>((1.0 - (d(num5) + d(num6))) * d(point.x) +
                           (d(num7) - d(num12)) * d(point.y) +
                           (d(num8) + d(num11)) * d(point.z)),
        static_cast<float>((d(num7) + d(num12)) * d(point.x) +
                           (1.0 - (d(num4) + d(num6))) * d(point.y) +
                           (d(num9) - d(num10)) * d(point.z)),
        static_cast<float>((d(num8) - d(num11)) * d(point.x) +
                           (d(num9) + d(num10)) * d(point.y) +
                           (1.0 - (d(num4) + d(num5))) * d(point.z)),
    };
}

// Maths.QRotate.  Every statement is a managed float32 expression: `num` and
// `num2` are float locals and each result component is a float32 constructor
// argument, so each rounds exactly once even though it contains several
// multiplies and adds.
Vector3 qRotate(const Quaternion &rotation, Vector3 value) {
    value.x *= 2.0f;
    value.y *= 2.0f;
    value.z *= 2.0f;
    const auto qx = static_cast<double>(rotation.x);
    const auto qy = static_cast<double>(rotation.y);
    const auto qz = static_cast<double>(rotation.z);
    const auto qw = static_cast<double>(rotation.w);
    const auto vx = static_cast<double>(value.x);
    const auto vy = static_cast<double>(value.y);
    const auto vz = static_cast<double>(value.z);
    const auto num = static_cast<double>(static_cast<float>(qw * qw - 0.5));
    const auto num2 =
        static_cast<double>(static_cast<float>(qx * vx + qy * vy + qz * vz));
    return {
        static_cast<float>(vx * num + (qy * vz - qz * vy) * qw + qx * num2),
        static_cast<float>(vy * num + (qz * vx - qx * vz) * qw + qy * num2),
        static_cast<float>(vz * num + (qx * vy - qy * vx) * qw + qz * num2),
    };
}

// Maths.QInvRotate, same shape as qRotate with the cross term subtracted.
Vector3 inverseRotate(const Quaternion &rotation, Vector3 value) {
    value.x *= 2.0f;
    value.y *= 2.0f;
    value.z *= 2.0f;
    const auto qx = static_cast<double>(rotation.x);
    const auto qy = static_cast<double>(rotation.y);
    const auto qz = static_cast<double>(rotation.z);
    const auto qw = static_cast<double>(rotation.w);
    const auto vx = static_cast<double>(value.x);
    const auto vy = static_cast<double>(value.y);
    const auto vz = static_cast<double>(value.z);
    const auto num = static_cast<double>(static_cast<float>(qw * qw - 0.5));
    const auto num2 =
        static_cast<double>(static_cast<float>(qx * vx + qy * vy + qz * vz));
    return {
        static_cast<float>(vx * num - (qy * vz - qz * vy) * qw + qx * num2),
        static_cast<float>(vy * num - (qz * vx - qx * vz) * qw + qy * num2),
        static_cast<float>(vz * num - (qx * vy - qy * vx) * qw + qz * num2),
    };
}

// Transcribed from QuaternionScripting::AngleAxis at UnityPlayer.dll+F3130
// (image base 0x180000000; the address was resolved at runtime through
// mono_lookup_internal_call rather than guessed from the icall tables):
//
//     v8  = (x*x + y*y) + z*z          ; float, stepwise
//     v9  = sqrtf(v8)
//     if (v9 <= 1e-6f) return <identity constant>
//     v10 = ((a/360f + a/360f) * 3.1415927f) * 0.5f
//     v11 = cosf(v10)
//     v12 = sinf(v10) / v9
//     q   = (x*v12, y*v12, z*v12, v11)
//
// Two details matter and neither is guessable: the half angle comes from a
// float *division* by 360 and a float pi, so it is not `degrees * C` for any
// constant C; and the axis is not normalised up front -- its length divides the
// sine instead.
Quaternion angleAxis(float degrees, Vector3 axis) {
    const auto lengthSquared =
        (axis.x * axis.x + axis.y * axis.y) + axis.z * axis.z;
    const auto length = std::sqrt(lengthSquared);
    if (length <= 1.0E-06f) return {0.0f, 0.0f, 0.0f, 1.0f};
    auto half = degrees / 360.0f;
    half = half + half;
    half = half * 3.1415927410125732f;
    half = half * 0.5f;
    const auto cosine = ucrtCosf(half);
    const auto scale = ucrtSinf(half) / length;
    return {axis.x * scale, axis.y * scale, axis.z * scale, cosine};
}

namespace {

// Rodrigues rotation matrix taking `from` to `to`, transcribed from
// UnityPlayer.dll+599F70.  Row major, 3x3.  Only the general branch is
// reproduced exactly; the near-parallel and near-antiparallel branches are
// carried over as the reference writes them.
void fromToMatrix(const Vector3 &from, const Vector3 &to, float m[9]) {
    const auto cx = to.z * from.y - to.y * from.z;
    const auto cy = to.x * from.z - from.x * to.z;
    const auto cz = from.x * to.y - to.x * from.y;
    const auto cosine = (from.x * to.x + to.y * from.y) + from.z * to.z;
    if (cosine > 0.9999990000000025f) {
        m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f;
        m[3] = 0.0f; m[4] = 1.0f; m[5] = 0.0f;
        m[6] = 0.0f; m[7] = 0.0f; m[8] = 1.0f;
        return;
    }
    if (cosine < -0.9999990000000025f) {
        // Antiparallel: the reference picks a perpendicular axis and builds the
        // matrix from it.  Unreachable on the vein path (the `to` vectors are
        // never the exact negation of Vector3.up), so it falls back to the
        // 180-degree rotation rather than transcribing that branch blind.
        auto axis = cross(from, Vector3{1.0f, 0.0f, 0.0f});
        if (!(axis.sqrMagnitude() > Epsilon * Epsilon))
            axis = cross(from, Vector3{0.0f, 0.0f, 1.0f});
        const auto q = angleAxis(180.0f, axis);
        const auto xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        const auto xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        const auto wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
        m[0] = 1.0f - 2.0f * (yy + zz); m[1] = 2.0f * (xy - wz); m[2] = 2.0f * (xz + wy);
        m[3] = 2.0f * (xy + wz); m[4] = 1.0f - 2.0f * (xx + zz); m[5] = 2.0f * (yz - wx);
        m[6] = 2.0f * (xz - wy); m[7] = 2.0f * (yz + wx); m[8] = 1.0f - 2.0f * (xx + yy);
        return;
    }
    const auto h = (1.0f - cosine) / ((cx * cx + cy * cy) + cz * cz);
    const auto hx = cx * h;
    const auto hz = cz * h;
    const auto cyhx = cy * hx;
    const auto czhx = cz * hx;
    const auto cyhz = cy * hz;
    m[0] = cx * hx + cosine;
    m[1] = cz + cyhx;
    m[2] = czhx - cy;
    m[3] = cyhx - cz;
    m[4] = (cy * h) * cy + cosine;
    m[5] = cx + cyhz;
    m[6] = cy + czhx;
    m[7] = cyhz - cx;
    m[8] = cz * hz + cosine;
}

// UnityPlayer.dll+59D900: matrix to quaternion, then a float normalise.
Quaternion matrixToQuaternion(const float m[9]) {
    static constexpr int next[3] = {1, 2, 0};
    float q[4];  // x, y, z, w
    const auto trace = (m[0] + m[4]) + m[8];
    if (trace > 0.0f) {
        const auto s = std::sqrt(trace + 1.0f);
        const auto k = 0.5f / s;
        q[3] = s * 0.5f;
        q[0] = (m[5] - m[7]) * k;
        q[1] = (m[6] - m[2]) * k;
        q[2] = (m[1] - m[3]) * k;
    } else {
        int i = (m[4] > m[0]) ? 1 : 0;
        if (m[8] > m[4 * i]) i = 2;
        const auto j = next[i];
        const auto k2 = next[j];
        const auto s = std::sqrt(((m[4 * i] - m[4 * j]) - m[4 * k2]) + 1.0f);
        const auto k = 0.5f / s;
        q[i] = s * 0.5f;
        q[3] = (m[3 * j + k2] - m[3 * k2 + j]) * k;
        q[j] = (m[3 * i + j] + m[3 * j + i]) * k;
        q[k2] = (m[3 * i + k2] + m[3 * k2 + i]) * k;
    }
    const auto length = std::sqrt(
        ((q[0] * q[0] + q[1] * q[1]) + q[2] * q[2]) + q[3] * q[3]);
    return {q[0] / length, q[1] / length, q[2] / length, q[3] / length};
}

}

// QuaternionScripting::FromToRotation at UnityPlayer.dll+F2D20.  Both vectors
// are normalised in float (stepwise sum of squares, then sqrtf), a Rodrigues
// matrix is built, and that matrix is converted to a quaternion.  Rewriting it
// as the usual half-way-vector formula does not reproduce the engine.
Quaternion fromToRotation(Vector3 from, Vector3 to) {
    const auto fromLenSq = (from.x * from.x + from.y * from.y) + from.z * from.z;
    const auto fromLen = std::sqrt(fromLenSq);
    const auto toLenSq = (to.x * to.x + to.y * to.y) + to.z * to.z;
    const auto toLen = std::sqrt(toLenSq);
    if (fromLen < 0.0000099999997f || toLen < 0.0000099999997f)
        return {0.0f, 0.0f, 0.0f, 1.0f};
    const Vector3 fromUnit{from.x / fromLen, from.y / fromLen, from.z / fromLen};
    const Vector3 toUnit{to.x / toLen, to.y / toLen, to.z / toLen};
    float m[9];
    fromToMatrix(fromUnit, toUnit, m);
    return matrixToQuaternion(m);
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
        return rotateAroundAxis(normalA, axis, MathfPi * t) * magnitude;
    }

    auto axis = cross(a, b).normalized();
    auto angle = std::acos(dotValue) * t;
    return rotateAroundAxis(normalA, axis, angle) * magnitude;
}

}
