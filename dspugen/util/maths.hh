/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dspugen::util {

template<typename T>
inline T clamp01(T a) {
    if (a < T(0)) return T(0);
    if (a > T(1)) return T(1);
    return a;
}

template<typename T>
inline T lerp(T a, T b, T t) {
    auto delta = b - a;
    auto scaled = delta * clamp01(t);
    return a + scaled;
}

// Mathf.Lerp is a + (b - a) * Clamp01(t) returning float32, so the subtract,
// multiply and add are all evaluated in double and the result rounds once.
// The generic lerp<T> above rounds three times, which is wrong for float.
template <>
inline float lerp<float>(float a, float b, float t) {
    return static_cast<float>(static_cast<double>(a) +
                              (static_cast<double>(b) - static_cast<double>(a)) *
                                  static_cast<double>(clamp01(t)));
}

inline float mathfPow(float value, float power) {
    return static_cast<float>(std::pow(static_cast<double>(value), static_cast<double>(power)));
}

inline float mathfLog(float value) {
    return static_cast<float>(std::log(static_cast<double>(value)));
}

inline float mathfSqrt(float value) {
    return static_cast<float>(std::sqrt(static_cast<double>(value)));
}

// Banker's rounding, matching Math.Round / Mathf.Round. std::nearbyint rounds according
// to the current rounding mode, which is round-half-to-even here: nothing in this project
// calls fesetround, and tests/oracle.cc asserts std::fegetround() == FE_TONEAREST before
// emitting fixtures. That makes it bit-identical to the explicit floor/fraction/fmod form
// this replaces, including the cases that form special-cased: signed zero is preserved,
// non-finite values pass through, and values at or above 2^52 are already integral.
// Do not introduce fesetround anywhere in the generator without revisiting this.
inline double roundToEven(double value) {
    return std::nearbyint(value);
}

inline float mathfRound(float value) {
    return static_cast<float>(roundToEven(static_cast<double>(value)));
}

inline int mathfRoundToInt(float value) {
    return static_cast<int>(roundToEven(static_cast<double>(value)));
}

// Mathf.Clamp01 / Mathf.Clamp compare with < and > and return the input
// unchanged otherwise, so a NaN falls through instead of being pinned to a
// bound. Spelled out rather than deferring to std::clamp so the mapping from
// the reference call stays visible at the call site.
inline float mathfClamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

inline float mathfClamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// C# narrows a floating-point value to an integral type with conv.u2 / conv.u1,
// which truncates toward zero and keeps the low bits. The equivalent C++ cast is
// undefined once the truncated value leaves the target range, so the wrap is
// performed explicitly on a 64-bit integer first. Values that cannot be
// truncated at all (NaN, infinities) yield 0, matching Mono on x86-64.
inline std::uint16_t narrowToUInt16(double value) {
    if (!(value > -9.2233720368547758E18 && value < 9.2233720368547758E18))
        return 0;
    return static_cast<std::uint16_t>(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(value)) & 0xFFFFULL);
}

inline std::uint8_t narrowToUInt8(float value) {
    if (!(value > -9.2233720368547758E18f && value < 9.2233720368547758E18f))
        return 0;
    return static_cast<std::uint8_t>(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(value)) & 0xFFULL);
}

}
