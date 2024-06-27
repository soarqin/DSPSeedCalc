/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <cmath>

namespace dspugen {

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct VectorLF3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    [[nodiscard]] inline double sqrMagnitude() const {
        return x * x + y * y + z * z;
    }
    [[nodiscard]] inline double magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    [[nodiscard]] inline VectorLF3 normalized() const {
        double num = x * x + y * y + z * z;
        if (num < 1E-34) return VectorLF3{0, 0, 0};
        double num2 = std::sqrt(num);
        return VectorLF3{x / num2, y / num2, z / num2};
    }
};

inline VectorLF3 operator-(const VectorLF3 &lhs, const VectorLF3 &rhs) {
    return VectorLF3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

}
