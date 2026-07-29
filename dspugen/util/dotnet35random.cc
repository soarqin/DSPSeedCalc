/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "dotnet35random.hh"

#include <cstdlib>
#include <limits>

namespace dspugen::util {

DotNet35Random::DotNet35Random(int seed) {
    DSP_INSTRUMENT(
        auto &counters = instrumentation::local();
        ++counters.rngConstructions;
        counters.rngTraceHash ^= static_cast<std::uint32_t>(seed);
        counters.rngTraceHash *= 1099511628211ULL;
    );
    // The reference substitutes int.MaxValue for int.MinValue instead of taking its
    // absolute value, which C++ also leaves undefined for INT_MIN. Verified against
    // System.Random in the reference build's mscorlib.dll:
    //     161803398 - ((Seed == int.MinValue) ? int.MaxValue : Math.Abs(Seed))
    int num = 161803398 - (seed == std::numeric_limits<int>::min() ? int(MBIG) : std::abs(seed));
    // Slots 1..55 are all written before they can be read: 21 * i % 55 is a bijection
    // onto 1..54 for i in 1..54 because gcd(21, 55) == 1, and slot 55 is assigned
    // directly. Slot 0 is never read at all, because sample() pre-increments and wraps
    // to 1, so zeroing the whole array on construction was dead work. It is still
    // written once here so the object is fully initialized.
    seedArray[0] = 0;
    seedArray[55] = num;
    int num2 = 1;
    for (int i = 1; i < 55; i++) {
        int num3 = 21 * i % 55;
        seedArray[num3] = num2;
        num2 = num - num2;
        // Same wrap as `if (num2 < 0) num2 += MBIG`, without the unpredictable branch.
        num2 += MBIG & -static_cast<int>(num2 < 0);
        num = seedArray[num3];
    }

    // The reference form is
    //     for (k = 1; k < 56; k++) seedArray[k] -= seedArray[1 + (k + 30) % 55];
    // whose read index splits into two constant strides: k + 30 stays below 55 for
    // k <= 24, giving 1 + (k + 30) = k + 31, and wraps for k >= 25, giving
    // 1 + (k + 30 - 55) = k - 24. Writing the two ranges out preserves the ascending
    // k order and every individual subtraction, so the state is bit-identical, but the
    // constant strides let the compiler vectorize: the first range only reads slots
    // 32..55, which this pass never writes, and the second range reads slots 1..31 with
    // a loop-carried distance of 24, wide enough for any vector width in use.
    for (int j = 1; j < 5; j++) {
        for (int k = 1; k <= 24; k++) {
            int value = seedArray[k] - seedArray[k + 31];
            seedArray[k] = value + (MBIG & -static_cast<int>(value < 0));
        }
        for (int k = 25; k < 56; k++) {
            int value = seedArray[k] - seedArray[k - 24];
            seedArray[k] = value + (MBIG & -static_cast<int>(value < 0));
        }
    }
}

}
