/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <functional>

namespace maths {

template<typename T>
inline T clamp01(T a) {
    return std::clamp(a, T(0), T(1));
}

template<typename T>
inline T lerp(T a, T b, T t) {
    return a + (b - a) * clamp01(t);
}

}
