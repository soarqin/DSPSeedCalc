/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include <limits>
#include <cstdint>
#include <vector>

class DotNet35Random {
private:
    enum : int {
        MBIG = std::numeric_limits<int>::max(),
        MSEED = 161803398,
        MZ = 0
    };

    int inext = 0;
    int inextp = 31;
    int seedArray[56] = {};

private:
    inline double sample() {
        if (++inext >= 56) inext = 1;
        if (++inextp >= 56) inextp = 1;
        int num = seedArray[inext] - seedArray[inextp];
        if (num < 0) num += MBIG;
        seedArray[inext] = num;
        return (double)num * 4.6566128752457969E-10;
    }

public:
    explicit DotNet35Random(int seed);
    inline int next() {
        return (int)(sample() * 2147483647.0);
    }

    inline int next(int maxValue) {
        if (maxValue < 0) return 0;
        return (int)(sample() * (double)maxValue);
    }

    inline int next(int minValue, int maxValue) {
        if (minValue > maxValue) return minValue;
        auto num = maxValue - minValue;
        if (num <= 1) return minValue;
        return (int)((uint64_t)(uint32_t)(sample() * double(num)) + (uint64_t)(int64_t)minValue);
    }
    template<typename T>
    inline void nextBytes(std::vector<uint8_t> &buffer) {
        for (auto &c: buffer) {
            c = (uint8_t)(sample() * 256.0);
        }
    }
    inline double nextDouble() {
        return sample();
    }
};
