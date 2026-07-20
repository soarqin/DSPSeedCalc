#include "galaxy.hh"
#include "protoset.hh"
#include "settings.hh"
#include "util/dotnet35random.hh"

#include <cfenv>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

template<typename To, typename From>
To bits(const From &value) {
    static_assert(sizeof(To) == sizeof(From));
    To result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

template<typename T>
std::string hexBits(T value) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(sizeof(T) * 2) << value;
    return stream.str();
}

void printFloat(const char *name, float value) {
    std::cout << ' ' << name << '=' << hexBits(bits<std::uint32_t>(value));
}

void printDouble(const char *name, double value) {
    std::cout << ' ' << name << '=' << hexBits(bits<std::uint64_t>(value));
}

void dumpRng(int seed) {
    dspugen::util::DotNet35Random ints(seed);
    std::cout << "rng-next seed=" << seed;
    for (int i = 0; i < 100; ++i) std::cout << ' ' << ints.next();
    std::cout << '\n';

    dspugen::util::DotNet35Random doubles(seed);
    std::cout << "rng-double seed=" << seed;
    for (int i = 0; i < 100; ++i) {
        std::cout << ' ' << hexBits(bits<std::uint64_t>(doubles.nextDouble()));
    }
    std::cout << '\n';
}

void dumpPositions(int seed, int starCount) {
    std::vector<dspugen::VectorLF3> positions;
    int count = dspugen::Galaxy::GeneratePoses(
        dspugen::DefaultAlgoVersion, seed, starCount, positions);
    std::cout << "positions seed=" << seed << " requested=" << starCount
              << " count=" << count << '\n';
    for (std::size_t i = 0; i < positions.size(); ++i) {
        std::cout << "position index=" << i;
        printDouble("x", positions[i].x);
        printDouble("y", positions[i].y);
        printDouble("z", positions[i].z);
        std::cout << '\n';
    }
}

void dumpGalaxy(int seed, int starCount, bool names) {
    dspugen::settings = {};
    dspugen::settings.genName = names;
    dspugen::settings.hasPlanets = true;
    auto *galaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, seed, starCount);
    std::cout << "galaxy seed=" << seed << " stars=" << galaxy->starCount
              << " birth=" << galaxy->birthStarId
              << " habitable=" << galaxy->HabitableCount
              << " names=" << names << '\n';
    for (const auto *star: galaxy->stars) {
        std::cout << "star index=" << star->index << " id=" << star->id
                  << " seed=" << star->seed
                  << " type=" << static_cast<int>(star->type)
                  << " spectr=" << static_cast<int>(star->spectr);
        printDouble("x", star->position.x);
        printDouble("y", star->position.y);
        printDouble("z", star->position.z);
        printFloat("level", star->level);
        printFloat("mass", star->mass);
        printFloat("lifetime", star->lifetime);
        printFloat("age", star->age);
        printFloat("temperature", star->temperature);
        printFloat("luminosity", star->luminosity);
        printFloat("radius", star->radius);
        printFloat("habitableRadius", star->habitableRadius);
        printFloat("lightBalanceRadius", star->lightBalanceRadius);
        printFloat("orbitScaler", star->orbitScaler);
        printFloat("dysonRadius", star->dysonRadius);
        printFloat("resourceCoef", star->resourceCoef);
        std::cout << " name-size=" << star->name.size() << " name=" << std::quoted(star->name)
                  << " planets=" << star->planets.size() << '\n';
        for (auto *planet: star->planets) {
            planet->generateGas();
            std::cout << "planet id=" << planet->id
                      << " index=" << planet->index
                      << " number=" << planet->number
                      << " seed=" << planet->seed
                      << " type=" << static_cast<int>(planet->type)
                      << " singularity=" << planet->singularity
                      << " theme=" << planet->theme
                      << " algo=" << planet->algoId
                      << " orbitAround=" << planet->orbitAround
                      << " parent=" << (planet->orbitAroundPlanet ? planet->orbitAroundPlanet->id : 0)
                      << " orbitIndex=" << planet->orbitIndex;
            printFloat("orbitRadius", planet->orbitRadius);
            printFloat("radius", planet->radius);
            printFloat("scale", planet->scale);
            printFloat("habitableBias", planet->habitableBias);
            printFloat("sunDistance", planet->sunDistance);
            printFloat("temperatureBias", planet->temperatureBias);
            printFloat("luminosity", planet->luminosity);
            std::cout << " themeSeed=" << planet->themeSeed << " veins=";
            for (int spot: planet->veinSpot) std::cout << spot << ',';
            std::cout << " gas=";
            for (std::size_t i = 0; i < planet->gasItems.size(); ++i) {
                std::cout << planet->gasItems[i] << ':'
                          << hexBits(bits<std::uint32_t>(planet->gasSpeeds[i])) << ',';
            }
            std::cout << '\n';
        }
    }
    galaxy->release();
}

} // namespace

int main() {
#ifdef __FAST_MATH__
#error Fast math is not allowed in the parity oracle
#endif
    static_assert(sizeof(float) == 4);
    static_assert(sizeof(double) == 8);
    if (!std::numeric_limits<float>::is_iec559 ||
        !std::numeric_limits<double>::is_iec559 ||
        std::fegetround() != FE_TONEAREST) {
        std::cerr << "Unsupported floating-point environment\n";
        return 2;
    }

    dspugen::loadProtoSets();
    dspugen::Galaxy::initThread();
    dspugen::Star::initThread();
    dspugen::Planet::initThread();

    for (int seed: {0, 1, 12345678}) dumpRng(seed);
    for (int seed: {0, 12345678}) dumpPositions(seed, 64);
    for (int seed: {0, 12345678}) dumpGalaxy(seed, 64, false);
    dumpGalaxy(0, 64, true);
    dumpGalaxy(0, 64, false);

    dspugen::Planet::releaseThread();
    dspugen::Star::releaseThread();
    dspugen::Galaxy::releaseThread();
    return 0;
}
