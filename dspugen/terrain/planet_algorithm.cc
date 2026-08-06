#include "planet_algorithm.hh"

#include "simplex_noise.hh"
#include "../util/dotnet35random.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace dspugen::terrain {

namespace {

constexpr float Pi = 3.14159265358979323846f;

double levelize(double value, double level = 1.0, double offset = 0.0) {
    value = value / level - offset;
    auto whole = std::floor(value);
    auto fraction = value - whole;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    value = whole + fraction;
    return (value + offset) * level;
}

double levelize2(double value, double level = 1.0, double offset = 0.0) {
    value = value / level - offset;
    auto whole = std::floor(value);
    auto fraction = value - whole;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    value = whole + fraction;
    return (value + offset) * level;
}

double levelize3(double value, double level = 1.0, double offset = 0.0) {
    value = value / level - offset;
    auto whole = std::floor(value);
    auto fraction = value - whole;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    value = whole + fraction;
    return (value + offset) * level;
}

double levelize4(double value, double level = 1.0, double offset = 0.0) {
    value = value / level - offset;
    auto whole = std::floor(value);
    auto fraction = value - whole;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    fraction = (3.0 - fraction - fraction) * fraction * fraction;
    value = whole + fraction;
    return (value + offset) * level;
}

double lerpUnclamped(double a, double b, double t) {
    return a + (b - a) * t;
}

float mathfLerp(float a, float b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return a + (b - a) * t;
}

float mathfCos(float value) {
    return static_cast<float>(std::cos(static_cast<double>(value)));
}

float mathfPow(float value, float power) {
    return static_cast<float>(std::pow(static_cast<double>(value),
                                       static_cast<double>(power)));
}

float mathfSign(float value) {
    if (value > 0.0f) return 1.0f;
    if (value < 0.0f) return -1.0f;
    return 0.0f;
}

float terrainCurve(float value) {
    auto curve = mathfCos(value * Pi) * 1.1f;
    curve = mathfSign(curve) * mathfPow(curve, 4.0f);
    return std::clamp(curve, -1.0f, 1.0f);
}

struct SphericVector {
    double x;
    double y;
    double z;

    [[nodiscard]] double magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }
};

struct Circle {
    float x;
    float y;
    float z;
    float w;
};

// RandomTable is built from mscorlib System.Random, not the game's
// DotNet35Random (the two generators use different inextp values).  Keep this
// private compatibility copy local to terrain generation so the existing game
// RNG implementation remains unchanged.
class SystemRandomCompat {
public:
    explicit SystemRandomCompat(int seed) {
        auto num = 161803398 -
                   ((seed == std::numeric_limits<int>::min())
                        ? std::numeric_limits<int>::max()
                        : std::abs(seed));
        seedArray[55] = num;
        int num2 = 1;
        int index = 0;
        for (int i = 1; i < 55; ++i) {
            index += 21;
            if (index >= 55) index -= 55;
            seedArray[index] = num2;
            num2 = num - num2;
            if (num2 < 0) num2 += std::numeric_limits<int>::max();
            num = seedArray[index];
        }
        for (int j = 1; j < 5; ++j) {
            for (int k = 1; k < 56; ++k) {
                int index2 = k + 30;
                if (index2 >= 55) index2 -= 55;
                auto value = seedArray[k] - seedArray[1 + index2];
                if (value < 0) value += std::numeric_limits<int>::max();
                seedArray[k] = value;
            }
        }
    }

    [[nodiscard]] double nextDouble() {
        int index = ++inext;
        if (index >= 56) index = 1;
        int index2 = ++inextp;
        if (index2 >= 56) index2 = 1;
        auto value = seedArray[index] - seedArray[index2];
        if (value == std::numeric_limits<int>::max()) --value;
        if (value < 0) value += std::numeric_limits<int>::max();
        seedArray[index] = value;
        inext = index;
        inextp = index2;
        return static_cast<double>(value) * 4.656612875245797E-10;
    }

private:
    int inext = 0;
    int inextp = 21;
    int seedArray[56]{};
};

double randomNormal(SystemRandomCompat &random) {
    auto value = random.nextDouble();
    auto angle = random.nextDouble() * 3.14159265358979323846 * 2.0;
    return std::sqrt(-2.0 * std::log(1.0 - value)) * std::sin(angle);
}

const std::array<SphericVector, 65536> &sphericNormalTable() {
    static const auto table = [] {
        std::array<SphericVector, 65536> values{};
        SystemRandomCompat random(1001);
        for (auto &value : values) {
            double x;
            double y;
            double z;
            double normal;
            double lengthSquared;
            while (true) {
                x = random.nextDouble() * 2.0 - 1.0;
                y = random.nextDouble() * 2.0 - 1.0;
                z = random.nextDouble() * 2.0 - 1.0;
                normal = randomNormal(random);
                if (!(normal > 5.0) && !(normal < -5.0)) {
                    lengthSquared = x * x + y * y + z * z;
                    if (!(lengthSquared > 1.0) && !(lengthSquared < 1.0E-06))
                        break;
                }
            }
            auto scale = normal / std::sqrt(lengthSquared);
            x *= scale;
            y *= scale;
            z *= scale;
            value = {x, y, z};
        }
        return values;
    }();
    return table;
}

SphericVector nextSphericNormal(int &seed) {
    auto next = static_cast<std::uint32_t>(seed) + 1U;
    seed = static_cast<int>(next & 65535U);
    return sphericNormalTable()[static_cast<std::size_t>(seed)];
}

void normalizeUnityVector3(Vector3 &value) {
    auto dotValue = (value.x * value.x + value.y * value.y) +
                    value.z * value.z;
    auto length = static_cast<float>(std::sqrt(static_cast<double>(dotValue)));
    if (length > 1.0E-05f) {
        value.x /= length;
        value.y /= length;
        value.z /= length;
    } else {
        value = {};
    }
}

void generateTerrain0(PlanetAlgorithmContext &context) {
    auto height = static_cast<std::uint16_t>(
        static_cast<double>(context.radius) * 100.0);
    auto &heightData = context.data.heightData();
    auto &biomoData = context.data.biomoData();
    for (int i = 0; i < context.data.dataLength(); ++i) {
        heightData[static_cast<std::size_t>(i)] = height;
        biomoData[static_cast<std::size_t>(i)] = 0;
    }
}

void generateTerrain1(PlanetAlgorithmContext &context) {
    double num = 0.01;
    double num2 = 0.012;
    double num3 = 0.01;
    double num4 = 3.0;
    double num5 = -0.2;
    double num6 = 0.9;
    double num7 = 0.5;
    double num8 = 2.5;
    double num9 = 0.3;
    util::DotNet35Random random(context.seed);
    int num10 = random.next();
    int num11 = random.next();
    SimplexNoise simplexNoise(num10);
    SimplexNoise simplexNoise2(num11);
    auto &data = context.data;
    auto &heightData = data.heightData();
    auto &biomoData = data.biomoData();
    const auto &vertices = data.vertices();
    for (int i = 0; i < data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        double num12 = static_cast<double>(vertex.x * context.radius);
        double num13 = static_cast<double>(vertex.y * context.radius);
        double num14 = static_cast<double>(vertex.z * context.radius);
        double num15 = 0.0;
        double num16 = 0.0;
        double num17 = simplexNoise.Noise3DFBM(
            num12 * num, num13 * num2, num14 * num3, 6) * num4 + num5;
        double num18 = simplexNoise2.Noise3DFBM(
            num12 * 0.0025, num13 * 0.0025, num14 * 0.0025, 3) *
            num4 * num6 + num7;
        double num19 = ((num18 > 0.0) ? (num18 * 0.5) : num18);
        double num20 = num17 + num19;
        double num21 = ((num20 > 0.0) ? (num20 * 0.5) : (num20 * 1.6));
        double num22 = ((num21 > 0.0) ? levelize3(num21, 0.7)
                                       : levelize2(num21, 0.5));
        double num23 = simplexNoise2.Noise3DFBM(
            num12 * num * 2.5, num13 * num2 * 8.0,
            num14 * num3 * 2.5, 2) * 0.6 - 0.3;
        double num24 = num21 * num8 + num23 + num9;
        double num25 = ((num24 < 1.0) ? num24
                                      : ((num24 - 1.0) * 0.8 + 1.0));
        num15 = num22;
        num16 = num25;
        heightData[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(
                (static_cast<double>(context.radius) + num15 + 0.2) * 100.0);
        float biome = static_cast<float>(num16 * 100.0);
        biomoData[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(
            std::clamp(biome, 0.0f, 200.0f));
    }
}

void generateTerrain2(PlanetAlgorithmContext &context,
                      double modX,
                      double modY) {
    modX = (3.0 - modX - modX) * modX * modX;
    double num = 0.0035;
    double num2 = 0.025 * modX + 0.0035 * (1.0 - modX);
    double num3 = 0.0035;
    double num4 = 3.0;
    double num5 = 1.0 + 1.3 * modY;
    num *= num5;
    num2 *= num5;
    num3 *= num5;
    util::DotNet35Random random(context.seed);
    int num6 = random.next();
    int num7 = random.next();
    SimplexNoise simplexNoise(num6);
    SimplexNoise simplexNoise2(num7);
    auto &data = context.data;
    auto &heightData = data.heightData();
    auto &biomoData = data.biomoData();
    const auto &vertices = data.vertices();
    for (int i = 0; i < data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        double num8 = static_cast<double>(vertex.x * context.radius);
        double num9 = static_cast<double>(vertex.y * context.radius);
        double num10 = static_cast<double>(vertex.z * context.radius);
        double num11 = static_cast<double>(vertex.y);
        double num12 = 0.0;
        double num13 = 0.0;
        double num14 = simplexNoise.Noise3DFBM(
            num8 * num, num9 * num2, num10 * num3, 6, 0.45, 1.8);
        double num15 = simplexNoise2.Noise3DFBM(
            num8 * num * 2.0, num9 * num2 * 2.0,
            num10 * num3 * 2.0, 3);
        double value = num14 * num4 + num4 * 0.4;
        double num16 = 0.6 / (std::abs(value) + 0.6) - 0.25;
        double num17 = ((num16 < 0.0) ? (num16 * 0.3) : num16);
        double num18 = std::pow(std::abs(num11 * 1.01), 3.0) * 1.0;
        double num19 = ((num15 < 0.0) ? 0.0 : num15);
        double num20 = ((num18 > 1.0) ? 1.0 : num18);
        num12 = num17;
        num13 = num17 * 1.5 + num19 * 1.0 + num20;
        heightData[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(
                (static_cast<double>(context.radius) + num12 + 0.1) * 100.0);
        float biome = static_cast<float>(num13 * 100.0);
        biomoData[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(
            std::clamp(biome, 0.0f, 200.0f));
    }
}

void generateTerrain3(PlanetAlgorithmContext &context, double modX) {
    double num = 0.007;
    double num2 = 0.007;
    double num3 = 0.007;
    util::DotNet35Random random(context.seed);
    int num4 = random.next();
    int num5 = random.next();
    SimplexNoise simplexNoise(num4);
    SimplexNoise simplexNoise2(num5);
    auto &data = context.data;
    auto &heightData = data.heightData();
    auto &biomoData = data.biomoData();
    const auto &vertices = data.vertices();
    for (int i = 0; i < data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        double num6 = static_cast<double>(vertex.x * context.radius);
        double num7 = static_cast<double>(vertex.y * context.radius);
        double num8 = static_cast<double>(vertex.z * context.radius);
        num6 += std::sin(num7 * 0.15) * 3.0;
        num7 += std::sin(num8 * 0.15) * 3.0;
        num8 += std::sin(num6 * 0.15) * 3.0;
        double num9 = 0.0;
        double num10 = 0.0;
        double num11 = simplexNoise.Noise3DFBM(
            num6 * num * 1.0, num7 * num2 * 1.1,
            num8 * num3 * 1.0, 6, 0.5, 1.8);
        double num12 = simplexNoise2.Noise3DFBM(
            num6 * num * 1.3 + 0.5, num7 * num2 * 2.8 + 0.2,
            num8 * num3 * 1.3 + 0.7, 3) * 2.0;
        double num13 = simplexNoise2.Noise3DFBM(
            num6 * num * 6.0, num7 * num2 * 12.0,
            num8 * num3 * 6.0, 2) * 2.0;
        num13 = lerpUnclamped(num13, num13 * 0.1, modX);
        double num14 = simplexNoise2.Noise3DFBM(
            num6 * num * 0.8, num7 * num2 * 0.8,
            num8 * num3 * 0.8, 2) * 2.0;
        double num15 = num11 * 2.0 + 0.92;
        double num16 = num12 * static_cast<double>(
            std::abs(static_cast<float>(num14) + 0.5f));
        num15 += static_cast<double>(std::clamp(
            static_cast<float>(num16 - 0.35) * 1.0f, 0.0f, 1.0f));
        if (num15 < 0.0) num15 *= 2.0;
        double num17 = num15;
        num17 = levelize2(num15);
        if (num17 > 0.0) {
            num17 = levelize2(num15);
            num17 = lerpUnclamped(levelize4(num17), num17, modX);
        }
        double b = ((!(num17 > 0.0))
                        ? static_cast<double>(mathfLerp(
                              -1.0f, 0.0f, static_cast<float>(num17) + 1.0f))
                        : ((!(num17 > 1.0))
                               ? static_cast<double>(mathfLerp(
                                     0.0f, 0.3f, static_cast<float>(num17))) +
                                     num13 * 0.1
                               : ((num17 > 2.0)
                                      ? static_cast<double>(mathfLerp(
                                            1.2f, 2.0f,
                                            static_cast<float>(num17) - 2.0f)) +
                                            num13 * 0.12
                                      : static_cast<double>(mathfLerp(
                                            0.3f, 1.2f,
                                            static_cast<float>(num17) - 1.0f)) +
                                            num13 * 0.12)));
        double a = ((!(num17 > 0.0))
                        ? static_cast<double>(mathfLerp(
                              -4.0f, 0.0f, static_cast<float>(num17) + 1.0f))
                        : ((!(num17 > 1.0))
                               ? static_cast<double>(mathfLerp(
                                     0.0f, 0.3f, static_cast<float>(num17))) +
                                     num13 * 0.1
                               : ((num17 > 2.0)
                                      ? static_cast<double>(mathfLerp(
                                            1.4f, 2.7f,
                                            static_cast<float>(num17) - 2.0f)) +
                                            num13 * 0.12
                                      : static_cast<double>(mathfLerp(
                                            0.3f, 1.4f,
                                            static_cast<float>(num17) - 1.0f)) +
                                            num13 * 0.12)));
        double num18 = lerpUnclamped(a, b, modX);
        if (num15 < 0.0) num15 *= 2.0;
        if (num15 < 1.0) num15 = levelize(num15);
        num9 = num18;
        num10 = static_cast<double>(std::abs(static_cast<float>(num15)));
        num10 = ((num10 > 0.0) ? ((num10 > 2.0) ? 2.0 : num10) : 0.0);
        num10 += ((num10 > 1.8) ? ((0.0 - num13) * 0.8)
                                : (num13 * 0.2));
        heightData[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(
                (static_cast<double>(context.radius) + num9 + 0.2) * 100.0);
        float biome = static_cast<float>(num10 * 100.0);
        biomoData[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(
            std::clamp(biome, 0.0f, 200.0f));
    }
}

void generateTerrain4(PlanetAlgorithmContext &context) {
    double num = 0.007;
    double num2 = 0.007;
    double num3 = 0.007;
    util::DotNet35Random random(context.seed);
    int num4 = random.next();
    int num5 = random.next();
    SimplexNoise simplexNoise(num4);
    SimplexNoise simplexNoise2(num5);
    int num6 = random.next();
    std::array<Circle, 80> circles{};
    std::array<double, 80> heights{};
    for (int i = 0; i < 80; ++i) {
        auto vectorLF = nextSphericNormal(num6);
        Vector3 vector{static_cast<float>(vectorLF.x),
                       static_cast<float>(vectorLF.y),
                       static_cast<float>(vectorLF.z)};
        normalizeUnityVector3(vector);
        vector = vector * context.radius;
        float w = static_cast<float>(vectorLF.magnitude()) * 8.0f + 8.0f;
        w *= w;
        circles[static_cast<std::size_t>(i)] =
            {vector.x, vector.y, vector.z, w};
        heights[static_cast<std::size_t>(i)] =
            random.nextDouble() * 0.4 + 0.20000000298023224;
    }
    auto &data = context.data;
    auto &heightData = data.heightData();
    auto &biomoData = data.biomoData();
    const auto &vertices = data.vertices();
    for (int j = 0; j < data.dataLength(); ++j) {
        const auto &vertex = vertices[static_cast<std::size_t>(j)];
        double num7 = static_cast<double>(vertex.x * context.radius);
        double num8 = static_cast<double>(vertex.y * context.radius);
        double num9 = static_cast<double>(vertex.z * context.radius);
        double num10 = 0.0;
        double num11 = 0.0;
        double num12 = simplexNoise.Noise3DFBM(
            num7 * num, num8 * num2, num9 * num3, 4, 0.45, 1.8);
        double num13 = simplexNoise2.Noise3DFBM(
            num7 * num * 5.0, num8 * num2 * 5.0,
            num9 * num3 * 5.0, 4);
        double num14 = num12 * 1.5;
        double num15 = num13 * 0.2;
        double num16 = num14 * 0.08 + num15 * 2.0;
        double num17 = 0.0;
        for (int k = 0; k < 80; ++k) {
            const auto &circle = circles[static_cast<std::size_t>(k)];
            double num18 = static_cast<double>(circle.x) - num7;
            double num19 = static_cast<double>(circle.y) - num8;
            double num20 = static_cast<double>(circle.z) - num9;
            double num21 = num18 * num18 + num19 * num19 + num20 * num20;
            if (!(num21 > static_cast<double>(circle.w))) {
                double num22 = num21 / static_cast<double>(circle.w) +
                               num15 * 1.2;
                if (num22 < 0.0) num22 = 0.0;
                double num23 = num22 * num22;
                double num24 = num23 * num22;
                double num25 = -15.0 * num24 + 21.833333333334 * num23 -
                               7.533333333333 * num22 + 0.7 + num15;
                if (num25 < 0.0) num25 = 0.0;
                num25 *= num25;
                num25 *= heights[static_cast<std::size_t>(k)];
                num17 = ((num17 > num25) ? num17 : num25);
            }
        }
        num10 = num17 + num16 + 0.2;
        num11 = num14 * 2.0 + 0.8;
        num11 = ((num11 > 2.0) ? 2.0 : ((num11 < 0.0) ? 0.0 : num11));
        num11 += ((num11 > 1.5) ? (0.0 - num17) : num17) * 0.5;
        num11 += num13 * 0.63;
        heightData[static_cast<std::size_t>(j)] =
            static_cast<std::uint16_t>(
                (static_cast<double>(context.radius) + num10 + 0.1) * 100.0);
        float biome = static_cast<float>(num11 * 100.0);
        biomoData[static_cast<std::size_t>(j)] = static_cast<std::uint8_t>(
            std::clamp(biome, 0.0f, 200.0f));
    }
}

void generateTerrain5(PlanetAlgorithmContext &context) {
    util::DotNet35Random random(context.seed);
    int num = random.next();
    int num2 = random.next();
    SimplexNoise simplexNoise(num);
    SimplexNoise simplexNoise2(num2);
    auto &data = context.data;
    auto &heightData = data.heightData();
    auto &biomoData = data.biomoData();
    const auto &vertices = data.vertices();
    for (int i = 0; i < data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        double num3 = static_cast<double>(vertex.x * context.radius);
        double num4 = static_cast<double>(vertex.y * context.radius);
        double num5 = static_cast<double>(vertex.z * context.radius);
        double num6 = 0.0;
        double num7 = 0.0;
        double num8 = levelize(num3 * 0.007);
        double num9 = levelize(num4 * 0.007);
        double num10 = levelize(num5 * 0.007);
        num8 += simplexNoise.Noise(
            num3 * 0.05, num4 * 0.05, num5 * 0.05) * 0.04;
        num9 += simplexNoise.Noise(
            num4 * 0.05, num5 * 0.05, num3 * 0.05) * 0.04;
        num10 += simplexNoise.Noise(
            num5 * 0.05, num3 * 0.05, num4 * 0.05) * 0.04;
        double num11 = std::abs(simplexNoise2.Noise(num8, num9, num10));
        double num12 = (0.16 - num11) * 10.0;
        num12 = ((!(num12 > 0.0)) ? 0.0
                                   : ((num12 > 1.0) ? 1.0 : num12));
        num12 *= num12;
        double num13 = (simplexNoise.Noise3DFBM(
            num4 * 0.005, num5 * 0.005, num3 * 0.005, 4) + 0.22) * 5.0;
        num13 = ((!(num13 > 0.0)) ? 0.0
                                   : ((num13 > 1.0) ? 1.0 : num13));
        double num14 = std::abs(simplexNoise2.Noise3DFBM(
            num8 * 1.5, num9 * 1.5, num10 * 1.5, 2));
        double num15 = simplexNoise.Noise3DFBM(
            num5 * 0.06, num4 * 0.06, num3 * 0.06, 2);
        num6 -= num12 * 1.2 * num13;
        if (num6 >= 0.0) num6 += num11 * 0.25 + num14 * 0.6;
        num6 -= 0.1;
        num7 = num11 * 2.1;
        if (num7 < 0.0) num7 *= 5.0;
        num7 = ((!(num7 > -1.0)) ? -1.0
                                  : ((num7 > 2.0) ? 2.0 : num7));
        num7 += num15 * 0.6 * num7;
        double num16 = -0.3 - num6;
        if (num16 > 0.0) {
            double num17 = simplexNoise2.Noise(
                num3 * 0.16, num4 * 0.16, num5 * 0.16) - 1.0;
            num16 = ((num16 > 1.0) ? 1.0 : num16);
            num16 = (3.0 - num16 - num16) * num16 * num16;
            num6 = -0.3 - num16 * 3.700000047683716 +
                   num16 * num16 * num16 * num16 * num17 * 0.5;
        }
        heightData[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(
                (static_cast<double>(context.radius) + num6 + 0.2) * 100.0);
        float biome = static_cast<float>(num7 * 100.0);
        biomoData[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(
            std::clamp(biome, 0.0f, 200.0f));
    }
}

void generateTerrain6(PlanetAlgorithmContext &context) {
    util::DotNet35Random random(context.seed);
    int num = random.next();
    int num2 = random.next();
    SimplexNoise simplexNoise(num);
    SimplexNoise simplexNoise2(num2);
    auto &data = context.data;
    auto &heightData = data.heightData();
    auto &biomoData = data.biomoData();
    const auto &vertices = data.vertices();
    for (int i = 0; i < data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        double num3 = static_cast<double>(vertex.x * context.radius);
        double num4 = static_cast<double>(vertex.y * context.radius);
        double num5 = static_cast<double>(vertex.z * context.radius);
        double num6 = 0.0;
        double num7 = 0.0;
        double num8 = levelize(num3 * 0.007);
        double num9 = levelize(num4 * 0.007);
        double num10 = levelize(num5 * 0.007);
        num8 += simplexNoise.Noise(
            num3 * 0.05, num4 * 0.05, num5 * 0.05) * 0.04;
        num9 += simplexNoise.Noise(
            num4 * 0.05, num5 * 0.05, num3 * 0.05) * 0.04;
        num10 += simplexNoise.Noise(
            num5 * 0.05, num3 * 0.05, num4 * 0.05) * 0.04;
        double num11 = std::abs(simplexNoise2.Noise(num8, num9, num10));
        double num12 = (0.16 - num11) * 10.0;
        num12 = ((!(num12 > 0.0)) ? 0.0
                                   : ((num12 > 1.0) ? 1.0 : num12));
        num12 *= num12;
        double num13 = (simplexNoise.Noise3DFBM(
            num4 * 0.005, num5 * 0.005, num3 * 0.005, 4) + 0.22) * 5.0;
        num13 = ((!(num13 > 0.0)) ? 0.0
                                   : ((num13 > 1.0) ? 1.0 : num13));
        double num14 = std::abs(simplexNoise2.Noise3DFBM(
            num8 * 1.5, num9 * 1.5, num10 * 1.5, 2));
        num6 -= num12 * 1.2 * num13;
        if (num6 >= 0.0) num6 += num11 * 0.25 + num14 * 0.6;
        num6 -= 0.1;
        double num15 = -0.3 - num6;
        if (num15 > 0.0) {
            num15 = ((num15 > 1.0) ? 1.0 : num15);
            num15 = (3.0 - num15 - num15) * num15 * num15;
            num6 = -0.3 - num15 * 3.700000047683716;
        }
        double f = ((num12 > 0.30000001192092896)
                        ? num12
                        : 0.30000001192092896);
        f = levelize(f, 0.7);
        num6 = ((num6 > -0.800000011920929)
                    ? num6
                    : ((0.0 - f - num11) * 0.8999999761581421));
        num6 = ((num6 > -1.2000000476837158)
                    ? num6
                    : -1.2000000476837158);
        num7 = num6 * num12;
        num7 += num11 * 2.1 + 0.800000011920929;
        if (num7 > 1.7000000476837158 && num7 < 2.0) num7 = 2.0;
        heightData[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(
                (static_cast<double>(context.radius) + num6 + 0.2) * 100.0);
        float biome = static_cast<float>(num7 * 100.0);
        biomoData[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(
            std::clamp(biome, 0.0f, 200.0f));
    }
}

void generateTerrain8(PlanetAlgorithmContext &context,
                      double modX,
                      double modY) {
    double num = 0.002 * modX;
    double num2 = 0.002 * modX * modX * 6.66667;
    double num3 = 0.002 * modX;

    util::DotNet35Random random(context.seed);
    SimplexNoise simplexNoise(random.next());
    auto &data = context.data;
    auto &heightData = data.heightData();
    auto &biomoData = data.biomoData();
    const auto &vertices = data.vertices();
    for (int i = 0; i < data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        // The managed operands are Vector3/float values.  Their product is
        // rounded to single precision before the local is widened to double.
        double num4 = static_cast<double>(vertex.x * context.radius);
        double num5 = static_cast<double>(vertex.y * context.radius);
        double num6 = static_cast<double>(vertex.z * context.radius);
        float num9 = static_cast<float>(simplexNoise.Noise3DFBM(
            num4 * num, num5 * num2, num6 * num3, 6, 0.45, 1.8));
        num9 += 1.0f + static_cast<float>(modY) * 0.01f;
        num9 = std::clamp(num9, 0.0f, 2.0f);

        float num10;
        if (static_cast<double>(num9) < 1.0) {
            num10 = 1.0f - (terrainCurve(num9) + 1.0f) * 0.5f;
        } else {
            num10 = 2.0f - (terrainCurve(num9 - 1.0f) + 1.0f) * 0.5f;
        }
        double num7 = num10;
        double num8 = num10;
        num8 = ((num8 < 1.0) ? (std::max(num8 - 0.2, 0.0) * 1.25)
                              : num8);
        num8 = ((num8 > 1.0) ? std::min(num8 * num8, 2.0) : num8);
        num8 = levelize2(num8);
        heightData[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(
                (static_cast<double>(context.radius) + num7 + 0.1) * 100.0);
        float biome = static_cast<float>(num8 * 100.0);
        biome = std::clamp(biome, 0.0f, 200.0f);
        biomoData[static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>(biome);
    }
}

double remap(double sourceMin, double sourceMax, double targetMin,
             double targetMax, double value) {
    return (value - sourceMin) / (sourceMax - sourceMin) *
               (targetMax - targetMin) +
           targetMin;
}

double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

double clampDouble(double value, double minimum, double maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

double customMax(double lhs, double rhs) {
    return (!(lhs > rhs)) ? rhs : lhs;
}

void writeTerrain(PlanetAlgorithmContext &context, int index, double height,
                  double biome, double bias) {
    auto &heightData = context.data.heightData();
    auto &biomoData = context.data.biomoData();
    heightData[static_cast<std::size_t>(index)] = static_cast<std::uint16_t>(
        (static_cast<double>(context.radius) + height + bias) * 100.0);
    auto biomeFloat = static_cast<float>(biome * 100.0);
    biomoData[static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(
        std::clamp(biomeFloat, 0.0f, 200.0f));
}

void generateTerrain7(PlanetAlgorithmContext &context) {
    util::DotNet35Random random(context.seed);
    SimplexNoise first(random.next());
    SimplexNoise second(random.next());
    const auto &vertices = context.data.vertices();
    for (int i = 0; i < context.data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        const auto px = static_cast<double>(vertex.x * context.radius);
        const auto py = static_cast<double>(vertex.y * context.radius);
        const auto pz = static_cast<double>(vertex.z * context.radius);
        const auto a = first.Noise3DFBM(px * 0.008, py * 0.01, pz * 0.01, 6) *
                       3.0 - 2.4;
        const auto b = second.Noise3DFBM(px * 0.0025, py * 0.0025,
                                         pz * 0.0025, 3) * 3.0 * 0.9 + 0.5;
        const auto b1 = b > 0.0 ? b * 0.5 : b;
        const auto s = a + b1;
        const auto t = s > 0.0 ? s * 0.5 : s * 1.6;
        const auto h = t > 0.0 ? levelize3(t, 0.7) : levelize2(t, 0.5);
        const auto bn = second.Noise3DFBM(px * 0.008 * 2.5,
                                          py * 0.01 * 8.0,
                                          pz * 0.01 * 2.5, 2) * 0.6 - 0.3;
        const auto br = t * 2.5 + bn + 0.3;
        const auto biome = br < 1.0 ? br : (br - 1.0) * 0.8 + 1.0;
        writeTerrain(context, i, h, biome, 0.0);
    }
}

void generateTerrain9(PlanetAlgorithmContext &context, double modX,
                      double modY) {
    util::DotNet35Random random(context.seed);
    SimplexNoise first(random.next());
    SimplexNoise second(random.next());
    const auto &vertices = context.data.vertices();
    for (int i = 0; i < context.data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        const auto px = static_cast<double>(vertex.x * context.radius);
        const auto py = static_cast<double>(vertex.y * context.radius);
        const auto pz = static_cast<double>(vertex.z * context.radius);
        const auto a = first.Noise3DFBM(px * 0.01 * 0.75,
                                        py * 0.012 * 0.5,
                                        pz * 0.01 * 0.75, 6) * 3.0 - 0.2;
        const auto b = second.Noise3DFBM(px * 0.0025, py * 0.0025,
                                         pz * 0.0025, 3) * 3.0 * 0.9 + 0.5;
        const auto b1 = b > 0.0 ? b * 0.5 : b;
        const auto s = a + b1;
        const auto t = s > 0.0 ? s * 0.5 : s * 1.6;
        auto l = t > 0.0 ? levelize3(t, 0.7) : levelize2(t, 0.5);
        l += 0.618;
        l = l > -1.0 ? l * 1.5 : l * 4.0;
        // These values are intentionally evaluated even though the managed
        // implementation discards them; they do not consume RNG but preserve
        // the reference operation trace and exceptional-value behavior.
        const auto unused = second.Noise3DFBM(px * 0.01 * 2.5,
                                              py * 0.012 * 8.0,
                                              pz * 0.01 * 2.5, 2) * 0.6 - 0.3;
        const auto unusedHeight = t * 2.5 + unused + 0.3;
        (void)unusedHeight;
        const auto value = levelize(t + 0.7);
        const auto deepBase = first.Noise3DFBM(px * 0.01 * modX,
                                               py * 0.012 * modX,
                                               pz * 0.01 * modX, 6) *
                              3.0 - 0.2;
        const auto deepNoise = second.Noise3DFBM(px * 0.0025,
                                                 py * 0.0025,
                                                 pz * 0.0025, 3) *
                               3.0 * 0.9 + 0.5;
        const auto deepNoise2 = deepNoise > 0.0 ? deepNoise * 0.5 : deepNoise;
        auto deep = (deepBase + deepNoise2 + 5.0) * 0.13;
        deep = std::pow(deep, 6.0) * 24.0 - 24.0;
        const auto blend = l >= 0.0 - modY
                               ? 0.0
                               : std::pow(std::min(std::abs(l + modY) / 5.0,
                                                   1.0),
                                          1.0);
        auto height = l * (1.0 - blend) + deep * blend;
        height = height > 0.0 ? height * 0.5 : height;
        auto detail = second.Noise3DFBM(px * 0.01 * 1.5,
                                        py * 0.012 * 2.0,
                                        pz * 0.01 * 1.5, 6) * 3.0 - 0.2;
        detail = std::max(detail + 1.0, -0.99);
        detail = detail > 0.0 ? detail * 0.25 : detail;
        auto biome = std::max(value, 0.0);
        const auto biomeBoundary = static_cast<double>(std::clamp(
            static_cast<float>(biome - 1.0), 0.0f, 1.0f));
        biome = biome > 1.0 ? biomeBoundary * detail * 1.15 + 1.0 : biome;
        biome = std::min(biome, 2.0);
        writeTerrain(context, i, height, biome, 0.2);
    }
}

void generateTerrain10(PlanetAlgorithmContext &context) {
    util::DotNet35Random random(context.seed);
    SimplexNoise first(random.next());
    SimplexNoise second(random.next());
    SimplexNoise third(random.next());
    SimplexNoise fourth(random.next());
    int tableSeed = random.next();
    std::array<Circle, 10> ellipses{};
    std::array<double, 10> eccentricities{};
    std::array<double, 10> heights{};
    for (auto &ellipse: ellipses) {
        const auto normal = nextSphericNormal(tableSeed);
        Vector3 vector{static_cast<float>(normal.x), static_cast<float>(normal.y),
                       static_cast<float>(normal.z)};
        normalizeUnityVector3(vector);
        vector = vector * context.radius;
        ellipse.x = vector.x;
        ellipse.y = vector.y;
        ellipse.z = vector.z;
        ellipse.w = static_cast<float>(random.nextDouble() * 10.0 + 40.0);
        if (random.nextDouble() > 0.5)
            eccentricities[&ellipse - ellipses.data()] =
                remap(0.0, 1.0, 3.0, 5.0, random.nextDouble());
        else
            eccentricities[&ellipse - ellipses.data()] =
                remap(0.0, 1.0, 0.2, 1.0 / 3.0, random.nextDouble());
        heights[&ellipse - ellipses.data()] =
            remap(0.0, 1.0, 1.0, 2.0, random.nextDouble());
    }
    const auto &vertices = context.data.vertices();
    for (int i = 0; i < context.data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        auto px = static_cast<double>(vertex.x * context.radius);
        auto py = static_cast<double>(vertex.y * context.radius);
        auto pz = static_cast<double>(vertex.z * context.radius);
        auto qx = levelize(px * 0.007);
        auto qy = levelize(py * 0.007);
        auto qz = levelize(pz * 0.007);
        qx += third.Noise(px * 0.05, py * 0.05, pz * 0.05) * 0.04;
        qy += third.Noise(py * 0.05, pz * 0.05, px * 0.05) * 0.04;
        qz += third.Noise(pz * 0.05, px * 0.05, py * 0.05) * 0.04;
        auto ridge = std::abs(fourth.Noise(qx, qy, qz));
        auto cut = clamp01((0.16 - ridge) * 10.0);
        cut *= cut;
        auto broad = clamp01((third.Noise3DFBM(py * 0.005, pz * 0.005,
                                               px * 0.005, 4) +
                              0.22) *
                             5.0);
        auto fine = std::abs(fourth.Noise3DFBM(qx * 1.5, qy * 1.5,
                                               qz * 1.5, 2));
        auto ellipseNoise = second.Noise3DFBM(px * 0.007 * 5.0,
                                              py * 0.007 * 5.0,
                                              pz * 0.007 * 5.0, 4);
        auto ellipseNoiseScale = ellipseNoise * 0.2;
        double ellipseHeight = 0.0;
        for (int k = 0; k < 10; ++k) {
            const auto dx = static_cast<double>(ellipses[k].x) - px;
            const auto dy = static_cast<double>(ellipses[k].y) - py;
            const auto dz = static_cast<double>(ellipses[k].z) - pz;
            auto distance = eccentricities[k] * dx * dx + dy * dy + dz * dz;
            distance = remap(-1.0, 1.0, 0.2, 5.0, ellipseNoise) * distance;
            const auto radiusSquared = static_cast<double>(
                ellipses[k].w * ellipses[k].w);
            if (!(distance >= radiusSquared)) {
                const auto edge = static_cast<double>(1.0f - static_cast<float>(
                    std::sqrt(static_cast<double>(static_cast<float>(
                        distance / radiusSquared)))));
                const auto inverse = 1.0 - edge;
                auto profile = 1.0 - inverse * inverse * inverse * inverse +
                               ellipseNoiseScale * 2.0;
                if (profile < 0.0) profile = 0.0;
                ellipseHeight = customMax(ellipseHeight,
                                          heights[k] * profile);
            }
        }
        px += std::sin(py * 0.15) * 2.0;
        py += std::sin(pz * 0.15) * 2.0;
        pz += std::sin(px * 0.15) * 2.0;
        px *= 0.007;
        py *= 0.007;
        pz *= 0.007;
        auto base = static_cast<double>(mathfPow(
            static_cast<float>((first.Noise3DFBM(px * 0.6, py * 0.6,
                                                  pz * 0.6, 4, 0.5, 1.8) +
                                 1.0) *
                                0.5),
            1.3f));
        auto detail = remap(-1.0, 1.0, -0.1, 0.15,
                            second.Noise3DFBM(px * 6.0, py * 6.0, pz * 6.0, 5));
        auto shift = second.Noise3DFBM(px * 5.0 * 3.0, py * 5.0,
                                       pz * 5.0, 1);
        auto detail2 = second.Noise3DFBM(px * 5.0 * 3.0 + shift * 0.3,
                                         py * 5.0 + shift * 0.3,
                                         pz * 5.0 + shift * 0.3, 5) * 0.1;
        base = static_cast<double>(static_cast<float>(levelize4(base)));
        base = std::min(1.0, base);
        if (!(base > 0.8)) base += (!(base > 0.4) ? detail : detail2);
        auto height = base * 2.5 - base * ellipseHeight;
        height = customMax(height, detail * 2.0);
        const auto attenuation = (2.0 - height) / 2.0;
        height -= cut * 1.2 * broad * attenuation;
        if (height >= 0.0) height += (ridge * 0.25 + fine * 0.6) * attenuation;
        height -= 0.1;
        auto biome = std::abs(customMax(height, -1.0));
        if (base < 0.4)
            biome += remap(-1.0, 1.0, -0.2, 0.2,
                           first.Noise3DFBM(px * 2.0 + 100.0,
                                            py * 2.0 + 100.0,
                                            pz * 2.0 + 100.0, 5));
        writeTerrain(context, i, height, biome, 0.1);
    }
}

void generateTerrain11(PlanetAlgorithmContext &context, double modX,
                       double modY) {
    util::DotNet35Random random(context.seed);
    SimplexNoise first(random.next());
    SimplexNoise second(random.next());
    SimplexNoise third(random.next());
    const auto &vertices = context.data.vertices();
    const auto mx = 0.002 * modX;
    const auto my = 0.002 * modX * 4.0;
    for (int i = 0; i < context.data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        const auto px = static_cast<double>(vertex.x * context.radius);
        const auto py = static_cast<double>(vertex.y * context.radius);
        const auto pz = static_cast<double>(vertex.z * context.radius);
        const auto low = second.Noise3DFBM(px * 0.007 * 4.0,
                                           py * 0.007 * 8.0,
                                           pz * 0.007 * 4.0, 3);
        auto x = first.Noise3DFBM(px * 0.007 * 0.6,
                                  py * 0.007 * 1.5 * 2.5,
                                  pz * 0.007 * 0.6, 6, 0.45, 1.8) * 0.95 +
                 low * 0.05;
        x = remap(-1.0, 1.0, 0.0, 1.0, x);
        x = std::pow(x, modY);
        x += 1.0;
        x = levelize2(x);
        auto x2 = third.Noise3DFBM(px * mx, py * my, pz * mx, 5, 0.55);
        x2 = remap(-1.0, 1.0, 0.0, 1.0, x2);
        x2 = std::pow(x2, 0.65);
        const auto biome = levelize3(x2) * x;
        auto height = (biome - 0.4) * 0.9;
        height = std::max(-0.3, height);
        writeTerrain(context, i, height, biome, 0.0);
    }
}

double curveEvaluate(double value) {
    value /= 0.6;
    if (value >= 1.0) return 0.0;
    const auto oneMinus = 1.0 - value;
    return std::pow(oneMinus, 3.0) + std::pow(oneMinus, 2.0) * 3.0 * value;
}

void generateTerrain12(PlanetAlgorithmContext &context, double modX,
                       double modY) {
    util::DotNet35Random random(context.seed);
    SimplexNoise first(random.next());
    SimplexNoise second(random.next());
    const auto scale = 1.1 * modX;
    const auto &vertices = context.data.vertices();
    for (int i = 0; i < context.data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        const auto latitude = std::abs(std::asin(static_cast<double>(vertex.y))) *
                              2.0 / 3.14159265358979323846;
        const auto px = static_cast<double>(vertex.x);
        const auto py = static_cast<double>(vertex.y) * 2.5 * modY;
        const auto pz = static_cast<double>(vertex.z);
        const auto warp = second.Noise3DFBM(px * scale, py * scale,
                                            pz * scale, 3, 0.4) * 0.2;
        const auto ridge = first.RidgedNoise(px * scale,
                                             py * scale - warp,
                                             pz * scale, 6, 0.7, 2.0, 0.8);
        auto fbm = first.Noise3DFBMInitialAmp(
            px * scale, py * scale - warp, pz * scale, 6, 0.6, 2.0, 0.7);
        fbm *= ridge + fbm;
        fbm = 0.2 + 8.0 * fbm * ridge;
        auto value = fbm + 0.5;
        value = remap(-8.0, 8.0, 0.0, 1.0, value);
        value = clamp01(value);
        value += 0.5;
        value = std::pow(value, 1.5);
        value -= curveEvaluate(static_cast<double>(
            static_cast<float>(latitude * 0.9)));
        const auto biomeRaw = value * 2.0;
        auto height = clampDouble(biomeRaw, 0.0, 2.0) * 1.1 - 0.2;
        writeTerrain(context, i, height, biomeRaw, 0.0);
    }
}

void generateTerrain13(PlanetAlgorithmContext &context, double modX,
                       double modY) {
    util::DotNet35Random random(context.seed);
    SimplexNoise noise(random.next());
    const auto scale = 0.007 * modX;
    const auto &vertices = context.data.vertices();
    for (int i = 0; i < context.data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        const auto px = static_cast<double>(vertex.x * context.radius);
        const auto py = static_cast<double>(vertex.y * context.radius);
        const auto pz = static_cast<double>(vertex.z * context.radius);
        auto value = remap(-1.0, 1.0, 0.0, 1.0,
                           noise.Noise3DFBM(px * scale, py * scale,
                                            pz * scale, 6));
        value = std::pow(value, modY) * 3.0625;
        value = remap(0.0, 2.0, 0.0, 4.0, value);
        if (value < 1.0) value = std::pow(value, 2.0);
        value -= 0.2;
        auto biome = std::min(value, 4.0);
        const auto unused = std::max(1.0 - std::abs(1.0 - biome), 0.0);
        (void)unused;
        if (biome > 2.0) {
            if (!(biome > 3.0))
                biome = 2.0 - 1.0 * (biome - 2.0);
            else if (!(biome > 3.5))
                biome = 1.0;
            else
                biome = 1.0 + 2.0 * (biome - 3.5);
        }
        writeTerrain(context, i, biome, biome, 0.1);
    }
}

void generateTerrain14(PlanetAlgorithmContext &context) {
    constexpr double xScale = 0.007;
    constexpr double yScale = 0.007;
    constexpr double zScale = 0.007;
    util::DotNet35Random random(context.seed);
    SimplexNoise first(random.next());
    SimplexNoise second(random.next());
    SimplexNoise third(random.next());
    SimplexNoise fourth(random.next());
    const auto &vertices = context.data.vertices();
    for (int i = 0; i < context.data.dataLength(); ++i) {
        const auto &vertex = vertices[static_cast<std::size_t>(i)];
        auto x = static_cast<double>(vertex.x * context.radius);
        auto y = static_cast<double>(vertex.y * context.radius);
        auto z = static_cast<double>(vertex.z * context.radius);
        auto levelX = levelize(x * 0.007 / 2.0);
        auto levelY = levelize(y * 0.007 / 2.0);
        auto levelZ = levelize(z * 0.007 / 2.0);
        levelX += third.Noise(x * 0.05, y * 0.05, z * 0.05) * 0.04;
        levelY += third.Noise(y * 0.05, z * 0.05, x * 0.05) * 0.04;
        levelZ += third.Noise(z * 0.05, x * 0.05, y * 0.05) * 0.04;
        const auto lavaNoise = std::abs(fourth.Noise(levelX, levelY, levelZ));
        auto lava = (0.12 - lavaNoise) * 10.0;
        lava = (!(lava > 0.0)) ? 0.0 : ((lava > 1.0) ? 1.0 : lava);
        lava *= lava;
        auto broad = (third.Noise3DFBM(y * 0.005, z * 0.005,
                                       x * 0.005, 4) +
                      0.22) *
                     5.0;
        broad = (!(broad > 0.0)) ? 0.0
                                 : ((broad > 1.0) ? 1.0 : broad);
        (void)std::abs(fourth.Noise3DFBM(levelX * 1.5, levelY * 1.5,
                                         levelZ * 1.5, 2));
        x += std::sin(y * 0.15) * 3.0;
        y += std::sin(z * 0.15) * 3.0;
        z += std::sin(x * 0.15) * 3.0;
        double height = 0.0;
        double biome = 0.0;
        const auto base = first.Noise3DFBM(
            x * xScale * 1.0, y * yScale * 1.1, z * zScale * 1.0,
            6, 0.5, 1.8);
        const auto detail = second.Noise3DFBM(
            x * xScale * 1.3 + 0.5, y * yScale * 2.8 + 0.2,
            z * zScale * 1.3 + 0.7, 3) *
                            2.0;
        const auto fine = second.Noise3DFBM(
            x * xScale * 6.0, y * yScale * 12.0,
            z * zScale * 6.0, 2) *
                          2.0;
        const auto shape = second.Noise3DFBM(
            x * xScale * 0.8, y * yScale * 0.8,
            z * zScale * 0.8, 2) *
                           2.0;
        auto rawHeight = base * 2.0 + 0.92;
        const auto detailShape = detail * static_cast<double>(
            std::abs(static_cast<float>(shape) + 0.5f));
        rawHeight += static_cast<double>(std::clamp(
            static_cast<float>(detailShape - 0.35) * 1.0f, 0.0f, 1.0f));
        if (rawHeight < 0.0) rawHeight = 0.0;
        auto levelHeight = rawHeight;
        levelHeight = levelize2(rawHeight);
        if (levelHeight > 0.0) {
            levelHeight = levelize2(rawHeight);
            levelHeight = levelize4(levelHeight);
        }
        const auto terrainHeight =
            (!(levelHeight > 0.0))
                ? static_cast<double>(mathfLerp(
                      -4.0f, 0.0f, static_cast<float>(levelHeight) + 1.0f))
                : (!(levelHeight > 1.0))
                      ? static_cast<double>(mathfLerp(
                            0.0f, 0.3f, static_cast<float>(levelHeight))) +
                            fine * 0.1
                      : (levelHeight > 2.0)
                            ? static_cast<double>(mathfLerp(
                                  1.4f, 2.7f,
                                  static_cast<float>(levelHeight) - 2.0f)) +
                                  fine * 0.12
                            : static_cast<double>(mathfLerp(
                                  0.3f, 1.4f,
                                  static_cast<float>(levelHeight) - 1.0f)) +
                                  fine * 0.12;
        if (rawHeight < 0.0) rawHeight *= 2.0;
        if (rawHeight < 1.0) rawHeight = levelize(rawHeight);
        height -= lava * 1.2 * broad;
        if (height >= 0.0) height = terrainHeight;
        height -= 0.1;
        biome = static_cast<double>(std::abs(static_cast<float>(rawHeight)));
        auto blend = static_cast<double>(std::clamp(
            static_cast<float>((0.0 - height + 2.0) / 2.5), 0.0f, 1.0f));
        blend = std::pow(blend, 10.0);
        biome = (1.0 - blend) * biome + blend * 2.0;
        biome = (!(biome > 0.0)) ? 0.0
                                 : ((biome > 2.0) ? 2.0 : biome);
        biome += ((biome > 1.8) ? ((0.0 - fine) * 0.8)
                                : (fine * 0.2)) *
                 (1.0 - blend);
        auto depression = -0.3 - height;
        if (depression > 0.0) {
            const auto depressionNoise =
                second.Noise(x * 0.16, y * 0.16, z * 0.16) - 1.0;
            depression = (depression > 1.0) ? 1.0 : depression;
            depression = (3.0 - depression - depression) * depression *
                         depression;
            height = -0.3 - depression * 10.0 + depression * depression *
                     depression * depression * depressionNoise * 0.5;
        }
        writeTerrain(context, i, height, biome, 0.2);
    }
}

}

TerrainGenerationResult generateTerrain(int algorithmId,
                                         PlanetAlgorithmContext &context,
                                         double modX,
                                         double modY) {
    switch (algorithmId) {
    case 0:
        generateTerrain0(context);
        return TerrainGenerationResult::Generated;
    case 1:
        generateTerrain1(context);
        return TerrainGenerationResult::Generated;
    case 2:
        generateTerrain2(context, modX, modY);
        return TerrainGenerationResult::Generated;
    case 3:
        generateTerrain3(context, modX);
        return TerrainGenerationResult::Generated;
    case 4:
        generateTerrain4(context);
        return TerrainGenerationResult::Generated;
    case 5:
        generateTerrain5(context);
        return TerrainGenerationResult::Generated;
    case 6:
        generateTerrain6(context);
        return TerrainGenerationResult::Generated;
    case 7:
        generateTerrain7(context);
        return TerrainGenerationResult::Generated;
    case 8:
        generateTerrain8(context, modX, modY);
        return TerrainGenerationResult::Generated;
    case 9:
        generateTerrain9(context, modX, modY);
        return TerrainGenerationResult::Generated;
    case 10:
        generateTerrain10(context);
        return TerrainGenerationResult::Generated;
    case 11:
        generateTerrain11(context, modX, modY);
        return TerrainGenerationResult::Generated;
    case 12:
        generateTerrain12(context, modX, modY);
        return TerrainGenerationResult::Generated;
    case 13:
        generateTerrain13(context, modX, modY);
        return TerrainGenerationResult::Generated;
    case 14:
        generateTerrain14(context);
        return TerrainGenerationResult::Generated;
    default:
        return TerrainGenerationResult::UnsupportedAlgorithm;
    }
}

}
