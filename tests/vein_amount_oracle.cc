#include "galaxy.hh"
#include "planet.hh"
#include "protoset.hh"
#include "settings.hh"

#include <cstdint>
#include <iostream>

namespace {

void dumpGalaxy(int seed, int starCount) {
    dspugen::settings = {};
    dspugen::settings.hasPlanets = true;
    dspugen::settings.enableVeinAmounts = true;
    auto *galaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, seed, starCount);
    galaxy->ensureAllVeinAmounts();
    std::cout << "galaxy seed=" << seed << " stars=" << galaxy->starCount
              << " birthPlanet=" << galaxy->birthPlanetId << '\n';
    for (const auto *star: galaxy->stars) {
        for (const auto *planet: star->planets) {
            const auto *summary = planet->veinAmountSummary();
            std::cout << "planet id=" << planet->id
                      << " algo=" << planet->algoId
                      << " theme=" << planet->theme
                      << " spots=";
            for (int value: planet->veinSpot) std::cout << value << ',';
            std::cout << " nodes=";
            for (std::uint32_t value: summary->nodeCount)
                std::cout << value << ',';
            std::cout << " amounts=";
            for (std::int64_t value: summary->baseAmount)
                std::cout << value << ',';
            std::cout << '\n';
        }
    }
    galaxy->release();
}

}

int main() {
#ifdef __FAST_MATH__
#error Fast math is not allowed in the vein amount oracle
#endif
    dspugen::loadProtoSets();
    dspugen::Galaxy::initThread();
    dspugen::Star::initThread();
    dspugen::Planet::initThread();

    dumpGalaxy(0, 64);
    dumpGalaxy(1, 64);

    dspugen::Planet::releaseThread();
    dspugen::Star::releaseThread();
    dspugen::Galaxy::releaseThread();
    return 0;
}
