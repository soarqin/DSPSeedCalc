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

inline double roundToEven(double value) {
    if (!std::isfinite(value) || value == 0.0 || std::abs(value) >= 0x1p52) return value;
    auto lower = std::floor(value);
    auto fraction = value - lower;
    double result;
    if (fraction < 0.5) {
        result = lower;
    } else if (fraction > 0.5) {
        result = lower + 1.0;
    } else {
        result = std::fmod(std::abs(lower), 2.0) == 0.0 ? lower : lower + 1.0;
    }
    return std::copysign(result, value);
}

inline float mathfRound(float value) {
    return static_cast<float>(roundToEven(static_cast<double>(value)));
}

inline int mathfRoundToInt(float value) {
    return static_cast<int>(roundToEven(static_cast<double>(value)));
}

}
