#pragma once

#include <array>
#include <cstdint>

namespace dspugen::terrain {

// Port of the game's managed SimplexNoise class.  All coordinates and
// intermediate values intentionally remain double precision, matching the
// Mono implementation used by the recorded DSP build.
class SimplexNoise {
public:
    SimplexNoise();
    explicit SimplexNoise(int seed);

    [[nodiscard]] double Noise(double xin) const;
    [[nodiscard]] double Noise(double xin, double yin) const;
    [[nodiscard]] double Noise(double xin, double yin, double zin) const;
    [[nodiscard]] double Noise(double x, double y, double z, double w) const;

    [[nodiscard]] double Noise2DFBM(double x, double y, int nOctaves,
                                    double deltaAmp = 0.5,
                                    double deltaWLen = 2.0) const;
    [[nodiscard]] double Noise3DFBM(double x, double y, double z, int nOctaves,
                                    double deltaAmp = 0.5,
                                    double deltaWLen = 2.0) const;
    [[nodiscard]] double Noise3DFBMInitialAmp(double x, double y, double z,
                                              int nOctaves,
                                              double deltaAmp = 0.5,
                                              double deltaWLen = 2.0,
                                              double initialAmp = 0.5) const;
    [[nodiscard]] double RidgedNoise(double x, double y, double z, int nOctaves,
                                     double deltaAmp = 0.5,
                                     double deltaWLen = 2.0,
                                     double initialAmp = 0.5) const;

private:
    struct Grad {
        double x;
        double y;
        double z;
        double w;
    };

    static int fastFloor(double value);
    static double dot(const Grad &gradient, double x, double y);
    static double dot(const Grad &gradient, double x, double y, double z);
    static double dot(const Grad &gradient, double x, double y, double z,
                      double w);

    void initMember();
    void initMember(int seed);

    static const std::array<Grad, 12> grad3;
    static const std::array<Grad, 32> grad4;
    static const std::array<std::int16_t, 256> p;

    std::array<std::int16_t, 512> perm{};
    std::array<std::int16_t, 512> permMod12{};
};

}
