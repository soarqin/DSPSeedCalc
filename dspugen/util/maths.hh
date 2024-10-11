/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <algorithm>
#include <functional>

namespace dspugen::util {

template<typename T>
inline T clamp01(T a) {
    if (a < T(0)) return T(0);
    if (a > T(1)) return T(1);
    return a;
}

template<typename T>
inline T lerp(T a, T b, T t) {
    return a + (b - a) * double(clamp01(t));
}

}
