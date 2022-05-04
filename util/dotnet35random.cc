/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "dotnet35random.hh"

#include <cmath>

DotNet35Random::DotNet35Random(int seed) {
    int num = 161803398 - std::abs(seed);
    seedArray[55] = num;
    int num2 = 1;
    for (int i = 1; i < 55; i++) {
        int num3 = 21 * i % 55;
        seedArray[num3] = num2;
        num2 = num - num2;
        if (num2 < 0) num2 += MBIG;
        num = seedArray[num3];
    }

    for (int j = 1; j < 5; j++)
        for (int k = 1; k < 56; k++) {
            seedArray[k] -= seedArray[1 + (k + 30) % 55];
            if (seedArray[k] < 0) seedArray[k] += MBIG;
        }
}

int DotNet35Random::sampleInt() {
    if (++inext >= 56) inext = 1;
    if (++inextp >= 56) inextp = 1;
    int num = seedArray[inext] - seedArray[inextp];
    if (num < 0) num += MBIG;
    seedArray[inext] = num;
    return num;
}
