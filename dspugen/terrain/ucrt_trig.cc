#include "ucrt_trig.hh"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace dspugen::terrain {

namespace {

// Coefficients and thresholds read out of UnityPlayer.dll's FMA3 sinf / cosf.
// The game runs the FMA3 path (the CRT dispatches on CPU features), so every
// polynomial step below is a fused multiply-add, which is what makes these
// differ from a plain double-precision evaluation.
constexpr double SinC9 = 2.7557319223985893e-06;
constexpr double SinC7 = -0.0001984126984126984;
constexpr double SinC5 = 0.008333333333333333;
constexpr double SinC3 = -0.16666666666666666;

constexpr double CosC8 = -2.755731922398589e-07;
constexpr double CosC6 = 2.4801587301587298e-05;
constexpr double CosC4 = -0.0013888888888888887;
constexpr double CosC2 = 0.041666666666666664;

constexpr double PiOver4 = 0.7853981633974483;      // 3FE921FB54442D18
constexpr double SmallCut = 0.0078125;              // 3F80000000000000
constexpr double TinyCut = 0.0001220703125;         // 3F20000000000000
constexpr double TwoOverPi = 0.6366197723675814;    // 3FE45F306DC9C883
constexpr double PiOver2Hi = 1.5707963267341256;    // 3FF921FB54400000
constexpr double PiOver2Lo = 6.077100506506192e-11; // 3DD0B4611A626331
// 4170008AC0000000: above this the CRT switches to a Payne-Hanek reduction.
// Every angle on the vein path is far below it; the assert-free fallback keeps
// the fast reduction rather than silently returning a wrong value.
constexpr double FastReduceLimit = 16779436.0;

double absBits(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    bits &= 0x7FFFFFFFFFFFFFFFULL;
    double result;
    std::memcpy(&result, &bits, sizeof result);
    return result;
}

bool negative(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return (bits >> 63) != 0;
}

// sin on |x| <= pi/4, the vfmadd chain at 18185F6A0.
double sinPoly(double x) {
    const auto xx = x * x;
    auto p = std::fma(xx, SinC9, SinC7);
    p = std::fma(p, xx, SinC5);
    p = std::fma(p, xx, SinC3);
    return std::fma(p, x * xx, x);
}

// cos on |x| <= pi/4, the chain at 18185F0E0.  Note the leading 1 - x*x*0.5 is
// a plain subtract there, while the reduced-argument copy at 18185F8F0 forms it
// with an fma; both are reproduced where they are used.
double cosPoly(double x) {
    const auto xx = x * x;
    const auto base = 1.0 - xx * 0.5;
    auto p = std::fma(xx, CosC8, CosC6);
    p = std::fma(p, xx, CosC4);
    p = std::fma(p, xx, CosC2);
    return std::fma(p, xx * xx, base);
}

// The reduced-argument cos, which builds its leading term with an fma.
double cosPolyReduced(double x) {
    const auto xx = x * x;
    const auto base = std::fma(xx, -0.5, 1.0);
    auto p = std::fma(xx, CosC8, CosC6);
    p = std::fma(p, xx, CosC4);
    p = std::fma(p, xx, CosC2);
    return std::fma(p, xx * xx, base);
}

// Cody-Waite reduction from 18185F6EE: n = trunc(|x| * 2/pi + 0.5), then
// r = (|x| - n*PiOver2Hi) - n*PiOver2Lo.  Returns the region n & 3.
int reduce(double magnitude, double &reduced) {
    const auto scaled = std::fma(TwoOverPi, magnitude, 0.5);
    const auto n = static_cast<double>(static_cast<int>(scaled));
    const auto high = std::fma(-n, PiOver2Hi, magnitude);
    reduced = high - n * PiOver2Lo;
    return static_cast<int>(scaled) & 3;
}

}

float ucrtSinf(float value) {
    const auto x = static_cast<double>(value);
    const auto magnitude = absBits(x);
    if (magnitude <= PiOver4) {
        if (magnitude >= SmallCut) return static_cast<float>(sinPoly(x));
        if (magnitude < TinyCut) return value;
        return static_cast<float>(std::fma(-(x * 0.5), x * x, x));
    }
    if (magnitude >= FastReduceLimit) return static_cast<float>(std::sin(x));

    double reduced;
    const auto region = reduce(magnitude, reduced);
    auto result = ((region & 1) != 0) ? cosPolyReduced(reduced) : sinPoly(reduced);
    // 18185F935: negate when the quadrant says so, xor'd with the sign of the
    // original argument, because the reduction ran on |x|.
    if (((region == 2 || region == 3) ? 1 : 0) ^ (negative(x) ? 1 : 0))
        result = -result;
    return static_cast<float>(result);
}

float ucrtCosf(float value) {
    const auto x = static_cast<double>(value);
    const auto magnitude = absBits(x);
    if (magnitude <= PiOver4) {
        if (magnitude >= SmallCut) return static_cast<float>(cosPoly(x));
        if (magnitude < TinyCut) return 1.0f;
        return static_cast<float>(std::fma(-(x * 0.5), x, 1.0));
    }
    if (magnitude >= FastReduceLimit) return static_cast<float>(std::cos(x));

    double reduced;
    const auto region = reduce(magnitude, reduced);
    // cos is even, so the sign of the argument drops out and only the quadrant
    // matters: 0 -> cos, 1 -> -sin, 2 -> -cos, 3 -> sin.
    auto result = ((region & 1) != 0) ? sinPoly(reduced) : cosPolyReduced(reduced);
    if (region == 1 || region == 2) result = -result;
    return static_cast<float>(result);
}

}
