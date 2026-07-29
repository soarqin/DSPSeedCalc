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

}
