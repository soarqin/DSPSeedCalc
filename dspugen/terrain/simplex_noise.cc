#include "simplex_noise.hh"

#include "../util/dotnet35random.hh"

#include <algorithm>
#include <array>
#include <cmath>

namespace dspugen::terrain {

namespace {

const double F2 = 0.5 * (std::sqrt(3.0) - 1.0);
const double G2 = (3.0 - std::sqrt(3.0)) / 6.0;
const double F3 = 1.0 / 3.0;
const double G3 = 1.0 / 6.0;
const double F4 = (std::sqrt(5.0) - 1.0) / 4.0;
const double G4 = (5.0 - std::sqrt(5.0)) / 20.0;

}

const std::array<SimplexNoise::Grad, 12> SimplexNoise::grad3 = {{
    {1.0, 1.0, 0.0, 0.0},
    {-1.0, 1.0, 0.0, 0.0},
    {1.0, -1.0, 0.0, 0.0},
    {-1.0, -1.0, 0.0, 0.0},
    {1.0, 0.0, 1.0, 0.0},
    {-1.0, 0.0, 1.0, 0.0},
    {1.0, 0.0, -1.0, 0.0},
    {-1.0, 0.0, -1.0, 0.0},
    {0.0, 1.0, 1.0, 0.0},
    {0.0, -1.0, 1.0, 0.0},
    {0.0, 1.0, -1.0, 0.0},
    {0.0, -1.0, -1.0, 0.0},
}};

const std::array<SimplexNoise::Grad, 32> SimplexNoise::grad4 = {{
    {0.0, 1.0, 1.0, 1.0},
    {0.0, 1.0, 1.0, -1.0},
    {0.0, 1.0, -1.0, 1.0},
    {0.0, 1.0, -1.0, -1.0},
    {0.0, -1.0, 1.0, 1.0},
    {0.0, -1.0, 1.0, -1.0},
    {0.0, -1.0, -1.0, 1.0},
    {0.0, -1.0, -1.0, -1.0},
    {1.0, 0.0, 1.0, 1.0},
    {1.0, 0.0, 1.0, -1.0},
    {1.0, 0.0, -1.0, 1.0},
    {1.0, 0.0, -1.0, -1.0},
    {-1.0, 0.0, 1.0, 1.0},
    {-1.0, 0.0, 1.0, -1.0},
    {-1.0, 0.0, -1.0, 1.0},
    {-1.0, 0.0, -1.0, -1.0},
    {1.0, 1.0, 0.0, 1.0},
    {1.0, 1.0, 0.0, -1.0},
    {1.0, -1.0, 0.0, 1.0},
    {1.0, -1.0, 0.0, -1.0},
    {-1.0, 1.0, 0.0, 1.0},
    {-1.0, 1.0, 0.0, -1.0},
    {-1.0, -1.0, 0.0, 1.0},
    {-1.0, -1.0, 0.0, -1.0},
    {1.0, 1.0, 1.0, 0.0},
    {1.0, 1.0, -1.0, 0.0},
    {1.0, -1.0, 1.0, 0.0},
    {1.0, -1.0, -1.0, 0.0},
    {-1.0, 1.0, 1.0, 0.0},
    {-1.0, 1.0, -1.0, 0.0},
    {-1.0, -1.0, 1.0, 0.0},
    {-1.0, -1.0, -1.0, 0.0},
}};

std::array<std::int16_t, 256> SimplexNoise::p = {{
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95,
    96, 53, 194, 233, 7, 225, 140, 36, 103, 30,
    69, 142, 8, 99, 37, 240, 21, 10, 23, 190,
    6, 148, 247, 120, 234, 75, 0, 26, 197, 62,
    94, 252, 219, 203, 117, 35, 11, 32, 57, 177,
    33, 88, 237, 149, 56, 87, 174, 20, 125, 136,
    171, 168, 68, 175, 74, 165, 71, 134, 139, 48,
    27, 166, 77, 146, 158, 231, 83, 111, 229, 122,
    60, 211, 133, 230, 220, 105, 92, 41, 55, 46,
    245, 40, 244, 102, 143, 54, 65, 25, 63, 161,
    1, 216, 80, 73, 209, 76, 132, 187, 208, 89,
    18, 169, 200, 196, 135, 130, 116, 188, 159, 86,
    164, 100, 109, 198, 173, 186, 3, 64, 52, 217,
    226, 250, 124, 123, 5, 202, 38, 147, 118, 126,
    255, 82, 85, 212, 207, 206, 59, 227, 47, 16,
    58, 17, 182, 189, 28, 42, 223, 183, 170, 213,
    119, 248, 152, 2, 44, 154, 163, 70, 221, 153,
    101, 155, 167, 43, 172, 9, 129, 22, 39, 253,
    19, 98, 108, 110, 79, 113, 224, 232, 178, 185,
    112, 104, 218, 246, 97, 228, 251, 34, 242, 193,
    238, 210, 144, 12, 191, 179, 162, 241, 81, 51,
    145, 235, 249, 14, 239, 107, 49, 192, 214, 31,
    181, 199, 106, 157, 184, 84, 204, 176, 115, 121,
    50, 45, 127, 4, 150, 254, 138, 236, 205, 93,
    222, 114, 67, 29, 24, 72, 243, 141, 128, 195,
    78, 66, 215, 61, 156, 180,
}};

std::mutex SimplexNoise::pMutex;

int SimplexNoise::fastFloor(double value) {
    auto result = static_cast<int>(value);
    return value < static_cast<double>(result) ? result - 1 : result;
}

double SimplexNoise::dot(const Grad &gradient, double x, double y) {
    return gradient.x * x + gradient.y * y;
}

double SimplexNoise::dot(const Grad &gradient, double x, double y, double z) {
    return gradient.x * x + gradient.y * y + gradient.z * z;
}

double SimplexNoise::dot(const Grad &gradient, double x, double y, double z,
                         double w) {
    return gradient.x * x + gradient.y * y + gradient.z * z + gradient.w * w;
}

SimplexNoise::SimplexNoise() {
    initMember();
}

SimplexNoise::SimplexNoise(int seed) {
    initMember(seed);
}

void SimplexNoise::initMember() {
    for (int i = 0; i < 512; ++i) {
        perm[static_cast<std::size_t>(i)] = p[static_cast<std::size_t>(i & 0xff)];
        permMod12[static_cast<std::size_t>(i)] =
            static_cast<std::int16_t>(perm[static_cast<std::size_t>(i)] % 12);
    }
}

void SimplexNoise::initMember(int seed) {
    std::lock_guard<std::mutex> lock(pMutex);
    for (int i = 0; i < 256; ++i)
        p[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(i);

    util::DotNet35Random random(seed);
    for (int j = 0; j < 256; ++j) {
        int num = random.next(0, 256);
        auto &current = p[static_cast<std::size_t>(j)];
        auto &other = p[static_cast<std::size_t>(num)];
        auto value = current;
        current = other;
        other = value;
    }
    for (int k = 0; k < 512; ++k) {
        perm[static_cast<std::size_t>(k)] = p[static_cast<std::size_t>(k & 0xff)];
        permMod12[static_cast<std::size_t>(k)] =
            static_cast<std::int16_t>(perm[static_cast<std::size_t>(k)] % 12);
    }
}

double SimplexNoise::Noise(double xin) const {
    int num = fastFloor(xin);
    int num2 = num + 1;
    double num3 = xin - static_cast<double>(num);
    double num4 = num3 - 1.0;
    double num5 = 1.0 - num3 * num3;
    double num6 = num5 * num5;
    double num7 = num6 * num6 * grad3[static_cast<std::size_t>(perm[static_cast<std::size_t>(num & 0xff)] & 7)].x * num3;
    double num8 = 1.0 - num4 * num4;
    double num9 = num8 * num8;
    double num10 = num9 * num9 * grad3[static_cast<std::size_t>(perm[static_cast<std::size_t>(num2 & 0xff)] & 7)].x * num4;
    return (num7 + num10 - 0.0) * 3.1604938271604;
}

double SimplexNoise::Noise(double xin, double yin) const {
    double num = (xin + yin) * F2;
    int num2 = fastFloor(xin + num);
    int num3 = fastFloor(yin + num);
    double num4 = static_cast<double>(num2 + num3) * G2;
    double num5 = static_cast<double>(num2) - num4;
    double num6 = static_cast<double>(num3) - num4;
    double num7 = xin - num5;
    double num8 = yin - num6;
    int num9;
    int num10;
    if (num7 > num8) {
        num9 = 1;
        num10 = 0;
    } else {
        num9 = 0;
        num10 = 1;
    }
    double num11 = num7 - static_cast<double>(num9) + G2;
    double num12 = num8 - static_cast<double>(num10) + G2;
    double num13 = num7 - 1.0 + 2.0 * G2;
    double num14 = num8 - 1.0 + 2.0 * G2;
    int num15 = num2 & 0xff;
    int num16 = num3 & 0xff;
    int num17 = permMod12[static_cast<std::size_t>(num15 + perm[static_cast<std::size_t>(num16)])];
    int num18 = permMod12[static_cast<std::size_t>(num15 + num9 + perm[static_cast<std::size_t>(num16 + num10)])];
    int num19 = permMod12[static_cast<std::size_t>(num15 + 1 + perm[static_cast<std::size_t>(num16 + 1)])];
    double num20 = 0.5 - num7 * num7 - num8 * num8;
    double num21;
    if (num20 < 0.0) {
        num21 = 0.0;
    } else {
        num20 *= num20;
        num21 = num20 * num20 * dot(grad3[static_cast<std::size_t>(num17)], num7, num8);
    }
    double num22 = 0.5 - num11 * num11 - num12 * num12;
    double num23;
    if (num22 < 0.0) {
        num23 = 0.0;
    } else {
        num22 *= num22;
        num23 = num22 * num22 * dot(grad3[static_cast<std::size_t>(num18)], num11, num12);
    }
    double num24 = 0.5 - num13 * num13 - num14 * num14;
    double num25;
    if (num24 < 0.0) {
        num25 = 0.0;
    } else {
        num24 *= num24;
        num25 = num24 * num24 * dot(grad3[static_cast<std::size_t>(num19)], num13, num14);
    }
    return 70.1475 * (num21 + num23 + num25);
}

double SimplexNoise::Noise(double xin, double yin, double zin) const {
    double num = (xin + yin + zin) * F3;
    int num2 = fastFloor(xin + num);
    int num3 = fastFloor(yin + num);
    int num4 = fastFloor(zin + num);
    double num5 = static_cast<double>(num2 + num3 + num4) * G3;
    double num6 = static_cast<double>(num2) - num5;
    double num7 = static_cast<double>(num3) - num5;
    double num8 = static_cast<double>(num4) - num5;
    double num9 = xin - num6;
    double num10 = yin - num7;
    double num11 = zin - num8;
    int num12;
    int num13;
    int num14;
    int num15;
    int num16;
    int num17;
    if (num9 >= num10) {
        if (num10 >= num11) {
            num12 = 1; num13 = 0; num14 = 0;
            num15 = 1; num16 = 1; num17 = 0;
        } else if (num9 >= num11) {
            num12 = 1; num13 = 0; num14 = 0;
            num15 = 1; num16 = 0; num17 = 1;
        } else {
            num12 = 0; num13 = 0; num14 = 1;
            num15 = 1; num16 = 0; num17 = 1;
        }
    } else if (num10 < num11) {
        num12 = 0; num13 = 0; num14 = 1;
        num15 = 0; num16 = 1; num17 = 1;
    } else if (num9 < num11) {
        num12 = 0; num13 = 1; num14 = 0;
        num15 = 0; num16 = 1; num17 = 1;
    } else {
        num12 = 0; num13 = 1; num14 = 0;
        num15 = 1; num16 = 1; num17 = 0;
    }
    double num18 = num9 - static_cast<double>(num12) + G3;
    double num19 = num10 - static_cast<double>(num13) + G3;
    double num20 = num11 - static_cast<double>(num14) + G3;
    double num21 = num9 - static_cast<double>(num15) + 2.0 * G3;
    double num22 = num10 - static_cast<double>(num16) + 2.0 * G3;
    double num23 = num11 - static_cast<double>(num17) + 2.0 * G3;
    double num24 = num9 - 1.0 + 3.0 * G3;
    double num25 = num10 - 1.0 + 3.0 * G3;
    double num26 = num11 - 1.0 + 3.0 * G3;
    int num27 = num2 & 0xff;
    int num28 = num3 & 0xff;
    int num29 = num4 & 0xff;
    int num30 = permMod12[static_cast<std::size_t>(num27 + perm[static_cast<std::size_t>(num28 + perm[static_cast<std::size_t>(num29)])])];
    int num31 = permMod12[static_cast<std::size_t>(num27 + num12 + perm[static_cast<std::size_t>(num28 + num13 + perm[static_cast<std::size_t>(num29 + num14)])])];
    int num32 = permMod12[static_cast<std::size_t>(num27 + num15 + perm[static_cast<std::size_t>(num28 + num16 + perm[static_cast<std::size_t>(num29 + num17)])])];
    int num33 = permMod12[static_cast<std::size_t>(num27 + 1 + perm[static_cast<std::size_t>(num28 + 1 + perm[static_cast<std::size_t>(num29 + 1)])])];
    double num34 = 0.6 - num9 * num9 - num10 * num10 - num11 * num11;
    double num35;
    if (num34 < 0.0) {
        num35 = 0.0;
    } else {
        num34 *= num34;
        num35 = num34 * num34 * dot(grad3[static_cast<std::size_t>(num30)], num9, num10, num11);
    }
    double num36 = 0.6 - num18 * num18 - num19 * num19 - num20 * num20;
    double num37;
    if (num36 < 0.0) {
        num37 = 0.0;
    } else {
        num36 *= num36;
        num37 = num36 * num36 * dot(grad3[static_cast<std::size_t>(num31)], num18, num19, num20);
    }
    double num38 = 0.6 - num21 * num21 - num22 * num22 - num23 * num23;
    double num39;
    if (num38 < 0.0) {
        num39 = 0.0;
    } else {
        num38 *= num38;
        num39 = num38 * num38 * dot(grad3[static_cast<std::size_t>(num32)], num21, num22, num23);
    }
    double num40 = 0.6 - num24 * num24 - num25 * num25 - num26 * num26;
    double num41;
    if (num40 < 0.0) {
        num41 = 0.0;
    } else {
        num40 *= num40;
        num41 = num40 * num40 * dot(grad3[static_cast<std::size_t>(num33)], num24, num25, num26);
    }
    return 32.696434 * (num35 + num37 + num39 + num41);
}

double SimplexNoise::Noise(double x, double y, double z, double w) const {
    double num = (x + y + z + w) * F4;
    int num2 = fastFloor(x + num);
    int num3 = fastFloor(y + num);
    int num4 = fastFloor(z + num);
    int num5 = fastFloor(w + num);
    double num6 = static_cast<double>(num2 + num3 + num4 + num5) * G4;
    double num7 = static_cast<double>(num2) - num6;
    double num8 = static_cast<double>(num3) - num6;
    double num9 = static_cast<double>(num4) - num6;
    double num10 = static_cast<double>(num5) - num6;
    double num11 = x - num7;
    double num12 = y - num8;
    double num13 = z - num9;
    double num14 = w - num10;
    int num15 = 0;
    int num16 = 0;
    int num17 = 0;
    int num18 = 0;
    if (num11 > num12) ++num15; else ++num16;
    if (num11 > num13) ++num15; else ++num17;
    if (num11 > num14) ++num15; else ++num18;
    if (num12 > num13) ++num16; else ++num17;
    if (num12 > num14) ++num16; else ++num18;
    if (num13 > num14) ++num17; else ++num18;
    int num19 = (num15 >= 3) ? 1 : 0;
    int num20 = (num16 >= 3) ? 1 : 0;
    int num21 = (num17 >= 3) ? 1 : 0;
    int num22 = (num18 >= 3) ? 1 : 0;
    int num23 = (num15 >= 2) ? 1 : 0;
    int num24 = (num16 >= 2) ? 1 : 0;
    int num25 = (num17 >= 2) ? 1 : 0;
    int num26 = (num18 >= 2) ? 1 : 0;
    int num27 = (num15 >= 1) ? 1 : 0;
    int num28 = (num16 >= 1) ? 1 : 0;
    int num29 = (num17 >= 1) ? 1 : 0;
    int num30 = (num18 >= 1) ? 1 : 0;
    double num31 = num11 - static_cast<double>(num19) + G4;
    double num32 = num12 - static_cast<double>(num20) + G4;
    double num33 = num13 - static_cast<double>(num21) + G4;
    double num34 = num14 - static_cast<double>(num22) + G4;
    double num35 = num11 - static_cast<double>(num23) + 2.0 * G4;
    double num36 = num12 - static_cast<double>(num24) + 2.0 * G4;
    double num37 = num13 - static_cast<double>(num25) + 2.0 * G4;
    double num38 = num14 - static_cast<double>(num26) + 2.0 * G4;
    double num39 = num11 - static_cast<double>(num27) + 3.0 * G4;
    double num40 = num12 - static_cast<double>(num28) + 3.0 * G4;
    double num41 = num13 - static_cast<double>(num29) + 3.0 * G4;
    double num42 = num14 - static_cast<double>(num30) + 3.0 * G4;
    double num43 = num11 - 1.0 + 4.0 * G4;
    double num44 = num12 - 1.0 + 4.0 * G4;
    double num45 = num13 - 1.0 + 4.0 * G4;
    double num46 = num14 - 1.0 + 4.0 * G4;
    int num47 = num2 & 0xff;
    int num48 = num3 & 0xff;
    int num49 = num4 & 0xff;
    int num50 = num5 & 0xff;
    int num51 = perm[num47 + perm[num48 + perm[num49 + perm[num50]]]] % 32;
    int num52 = perm[num47 + num19 + perm[num48 + num20 + perm[num49 + num21 + perm[num50 + num22]]]] % 32;
    int num53 = perm[num47 + num23 + perm[num48 + num24 + perm[num49 + num25 + perm[num50 + num26]]]] % 32;
    int num54 = perm[num47 + num27 + perm[num48 + num28 + perm[num49 + num29 + perm[num50 + num30]]]] % 32;
    int num55 = perm[num47 + 1 + perm[num48 + 1 + perm[num49 + 1 + perm[num50 + 1]]]] % 32;
    double num56 = 0.6 - num11 * num11 - num12 * num12 - num13 * num13 - num14 * num14;
    double num57;
    if (num56 < 0.0) num57 = 0.0;
    else { num56 *= num56; num57 = num56 * num56 * dot(grad4[static_cast<std::size_t>(num51)], num11, num12, num13, num14); }
    double num58 = 0.6 - num31 * num31 - num32 * num32 - num33 * num33 - num34 * num34;
    double num59;
    if (num58 < 0.0) num59 = 0.0;
    else { num58 *= num58; num59 = num58 * num58 * dot(grad4[static_cast<std::size_t>(num52)], num31, num32, num33, num34); }
    double num60 = 0.6 - num35 * num35 - num36 * num36 - num37 * num37 - num38 * num38;
    double num61;
    if (num60 < 0.0) num61 = 0.0;
    else { num60 *= num60; num61 = num60 * num60 * dot(grad4[static_cast<std::size_t>(num53)], num35, num36, num37, num38); }
    double num62 = 0.6 - num39 * num39 - num40 * num40 - num41 * num41 - num42 * num42;
    double num63;
    if (num62 < 0.0) num63 = 0.0;
    else { num62 *= num62; num63 = num62 * num62 * dot(grad4[static_cast<std::size_t>(num54)], num39, num40, num41, num42); }
    double num64 = 0.6 - num43 * num43 - num44 * num44 - num45 * num45 - num46 * num46;
    double num65;
    if (num64 < 0.0) num65 = 0.0;
    else { num64 *= num64; num65 = num64 * num64 * dot(grad4[static_cast<std::size_t>(num55)], num43, num44, num45, num46); }
    return 27.29 * (num57 + num59 + num61 + num63 + num65);
}

double SimplexNoise::Noise2DFBM(double x, double y, int nOctaves,
                                double deltaAmp, double deltaWLen) const {
    double num = 0.0;
    double num2 = 0.5;
    for (int i = 0; i < nOctaves; ++i) {
        num += Noise(x, y) * num2;
        num2 *= deltaAmp;
        x *= deltaWLen;
        y *= deltaWLen;
    }
    return num;
}

double SimplexNoise::Noise3DFBM(double x, double y, double z, int nOctaves,
                                double deltaAmp, double deltaWLen) const {
    double num = 0.0;
    double num2 = 0.5;
    for (int i = 0; i < nOctaves; ++i) {
        num += Noise(x, y, z) * num2;
        num2 *= deltaAmp;
        x *= deltaWLen;
        y *= deltaWLen;
        z *= deltaWLen;
    }
    return num;
}

double SimplexNoise::Noise3DFBMInitialAmp(double x, double y, double z,
                                          int nOctaves, double deltaAmp,
                                          double deltaWLen, double initialAmp) const {
    double num = 0.0;
    double num2 = initialAmp;
    for (int i = 0; i < nOctaves; ++i) {
        num += Noise(x, y, z) * num2;
        num2 *= deltaAmp;
        x *= deltaWLen;
        y *= deltaWLen;
        z *= deltaWLen;
    }
    return num;
}

double SimplexNoise::RidgedNoise(double x, double y, double z, int nOctaves,
                                 double deltaAmp, double deltaWLen,
                                 double initialAmp) const {
    double num = 0.0;
    double num2 = initialAmp;
    for (int i = 0; i < nOctaves; ++i) {
        num += std::abs(Noise(x, y, z) * num2);
        num2 *= deltaAmp;
        x *= deltaWLen;
        y *= deltaWLen;
        z *= deltaWLen;
    }
    return num;
}

}
