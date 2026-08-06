#include "galaxy.hh"
#include "planet.hh"
#include "protoset.hh"
#include "settings.hh"
#include "terrain/planet_algorithm.hh"
#include "terrain/planet_raw_data.hh"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string hex64(std::uint64_t value) {
    char buffer[17];
    std::snprintf(buffer, sizeof(buffer), "%016llX",
                  static_cast<unsigned long long>(value));
    return buffer;
}

std::string floatBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    char buffer[9];
    std::snprintf(buffer, sizeof(buffer), "%08lX",
                  static_cast<unsigned long>(bits));
    return buffer;
}

std::string doubleBits(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return hex64(bits);
}

// Mirrors the format written by tools/vein-oracle-mod from inside the game, so
// tools/verify_vein_parity.py can compare the two directly.  Only fields the
// game itself produces appear here; veinSpot is a DSPSeedCalc estimate with no
// counterpart in the reference, so it stays in the regression dump instead.
void dumpGalaxyParity(int seed, int starCount) {
    dspugen::settings = {};
    dspugen::settings.hasPlanets = true;
    auto *galaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, seed, starCount);
    const auto batch = galaxy->ensureAllVeinAmounts(true);
    if (!batch.ok()) {
        std::cerr << "vein generation failed for seed " << seed << ", planet "
                  << batch.planetId << ", algorithm " << batch.algoId << ": "
                  << dspugen::veinAmountResultName(batch.result) << '\n';
        std::exit(1);
    }
    std::cout << "galaxy seed=" << seed << " stars=" << galaxy->starCount
              << " birthPlanet=" << galaxy->birthPlanetId << '\n';
    for (const auto *star: galaxy->stars) {
        std::cout << "star id=" << star->id
                  << " type=" << static_cast<int>(star->type)
                  << " spectr=" << static_cast<int>(star->spectr)
                  << " resourceCoef=" << floatBits(star->resourceCoef) << '\n';
    }
    for (const auto *star: galaxy->stars) {
        for (const auto *planet: star->planets) {
            const auto *summary = planet->veinAmountSummary();
            const auto &digest = planet->terrainDigest();
            std::cout << "planet id=" << planet->id
                      << " algo=" << planet->algoId
                      << " theme=" << planet->theme
                      << " modX=" << doubleBits(planet->modX)
                      << " modY=" << doubleBits(planet->modY)
                      << " radius=" << floatBits(planet->radius)
                      << " height=" << hex64(digest.height)
                      << " biomo=" << hex64(digest.biomo)
                      << " nodes=";
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

// Star and planet fields only, matching tools/vein-oracle-mod's "meta" mode.
// No terrain, so this scales to hundreds of seeds and covers the whole
// generator rather than just the vein path.
void dumpMeta(int seed, int starCount) {
    dspugen::settings = {};
    dspugen::settings.hasPlanets = true;
    dspugen::settings.genName = true;
    auto *galaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, seed, starCount);
    galaxy->ensureAllPlanets();
    galaxy->ensureAllGas();
    std::cout << "galaxy seed=" << galaxy->seed
              << " stars=" << galaxy->starCount
              << " birthStar=" << galaxy->birthStarId
              << " birthPlanet=" << galaxy->birthPlanetId
              << " habitable=" << galaxy->HabitableCount << '\n';
    for (const auto *star: galaxy->stars) {
        if (star == nullptr) continue;
        std::cout << "star id=" << star->id
                  << " index=" << star->index
                  << " seed=" << star->seed
                  << " type=" << static_cast<int>(star->type)
                  << " spectr=" << static_cast<int>(star->spectr)
                  << " name=" << star->name
                  << " posX=" << doubleBits(star->position.x)
                  << " posY=" << doubleBits(star->position.y)
                  << " posZ=" << doubleBits(star->position.z)
                  << " mass=" << floatBits(star->mass)
                  << " lifetime=" << floatBits(star->lifetime)
                  << " age=" << floatBits(star->age)
                  << " temperature=" << floatBits(star->temperature)
                  << " luminosity=" << floatBits(star->luminosity)
                  << " radius=" << floatBits(star->radius)
                  << " habitableRadius=" << floatBits(star->habitableRadius)
                  << " lightBalanceRadius=" << floatBits(star->lightBalanceRadius)
                  << " orbitScaler=" << floatBits(star->orbitScaler)
                  << " dysonRadius=" << floatBits(star->dysonRadius)
                  << " color=" << floatBits(star->color)
                  << " classFactor=" << floatBits(star->classFactor)
                  << " resourceCoef=" << floatBits(
                         const_cast<dspugen::Star *>(star)->updateResourceCoef())
                  << " planets=" << star->planets.size() << '\n';
        for (const auto *planet: star->planets) {
            if (planet == nullptr) continue;
            std::cout << "planet id=" << planet->id
                      << " index=" << planet->index
                      << " seed=" << planet->seed
                      << " number=" << planet->number
                      << " type=" << static_cast<int>(planet->type)
                      << " theme=" << planet->theme
                      << " algo=" << planet->algoId
                      << " singularity=" << planet->singularity
                      << " orbitAround=" << planet->orbitAround
                      << " orbitIndex=" << planet->orbitIndex
                      << " orbitRadius=" << floatBits(planet->orbitRadius)
                      << " radius=" << floatBits(planet->radius)
                      << " scale=" << floatBits(planet->scale)
                      << " habitableBias=" << floatBits(planet->habitableBias)
                      << " sunDistance=" << floatBits(planet->sunDistance)
                      << " temperatureBias=" << floatBits(planet->temperatureBias)
                      << " luminosity=" << floatBits(planet->luminosity)
                      << " orbitInclination=" << floatBits(planet->orbitInclination)
                      << " orbitLongitude=" << floatBits(planet->orbitLongitude)
                      << " orbitalPeriod=" << doubleBits(planet->orbitalPeriod)
                      << " orbitPhase=" << floatBits(planet->orbitPhase)
                      << " obliquity=" << floatBits(planet->obliquity)
                      << " rotationPeriod=" << doubleBits(planet->rotationPeriod)
                      << " rotationPhase=" << floatBits(planet->rotationPhase)
                      << " modX=" << doubleBits(planet->modX)
                      << " modY=" << doubleBits(planet->modY)
                      << " waterItemId=" << planet->waterItemId
                      << " waterHeight=" << floatBits(planet->waterHeight)
                      << " gas=";
            if (planet->gasItems.empty()) {
                std::cout << '-';
            } else {
                for (std::size_t i = 0; i < planet->gasItems.size(); ++i) {
                    if (i > 0) std::cout << ';';
                    std::cout << planet->gasItems[i] << ':'
                              << floatBits(planet->gasSpeeds[i]);
                }
            }
            std::cout << '\n';
        }
    }
    galaxy->release();
}

void dumpHeights(int seed, int starCount, const std::vector<int> &planetIds) {
    dspugen::settings = {};
    dspugen::settings.hasPlanets = true;
    auto *galaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, seed, starCount);
    galaxy->ensureAllPlanets();
    for (int planetId: planetIds) {
        auto *planet = galaxy->planetById(planetId);
        if (planet == nullptr) continue;
        dspugen::terrain::PlanetRawData raw(200);
        raw.calcVerts();
        dspugen::terrain::PlanetAlgorithmContext context{
            raw, planet->seed, planet->radius};
        dspugen::terrain::dispatchTerrain(planet->algoId, context,
                                          planet->modX, planet->modY);
        const auto stem = std::to_string(seed) + "-" + std::to_string(planetId) +
                          "-cpp.bin";
        const auto heightName = "height-" + stem;
        std::FILE *file = std::fopen(heightName.c_str(), "wb");
        std::fwrite(raw.heightData().data(), sizeof(std::uint16_t),
                    raw.heightData().size(), file);
        std::fclose(file);
        const auto biomoName = "biomo-" + stem;
        file = std::fopen(biomoName.c_str(), "wb");
        std::fwrite(raw.biomoData().data(), sizeof(std::uint8_t),
                    raw.biomoData().size(), file);
        std::fclose(file);
        std::cerr << "wrote " << heightName << " and " << biomoName
                  << " (algo " << planet->algoId << ")\n";
    }
    galaxy->release();
}

void dumpGalaxy(int seed, int starCount, bool includeTerrainDigest) {
    dspugen::settings = {};
    dspugen::settings.hasPlanets = true;
    auto *galaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, seed, starCount);
    galaxy->ensureAllVeinAmounts(includeTerrainDigest);
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

int main(int argc, char *argv[]) {
#ifdef __FAST_MATH__
#error Fast math is not allowed in the vein amount oracle
#endif
    auto parity = false;
    auto fullSummary = false;
    int parityFrom = 0;
    int parityTo = 1;
    int heightSeed = 0;
    int metaFrom = -1;
    int metaTo = -1;
    int metaStars = 64;
    std::vector<int> heightDump;
    std::string vertexGridPath;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--parity") {
            parity = true;
            // Optional "<first> <last>" range; defaults to the fixture's 0 and 1.
            if (i + 2 < argc && argv[i + 1][0] != '-') {
                parityFrom = std::stoi(argv[++i]);
                parityTo = std::stoi(argv[++i]);
            }
        }
        else if (argument == "--full-summary") {
            fullSummary = true;
        }
        // --meta <firstSeed> <lastSeed> [starCount]
        else if (argument == "--verts" && i + 1 < argc) {
            vertexGridPath = argv[++i];
        }
        else if (argument == "--meta" && i + 2 < argc) {
            metaFrom = std::stoi(argv[++i]);
            metaTo = std::stoi(argv[++i]);
            if (i + 1 < argc && argv[i + 1][0] != '-')
                metaStars = std::stoi(argv[++i]);
        }
        // --dump-height <seed> <planetId>...: writes each planet's raw
        // heightData so it can be compared byte for byte with the game dump.
        else if (argument == "--dump-height" && i + 2 < argc) {
            heightSeed = std::stoi(argv[++i]);
            while (i + 1 < argc) heightDump.push_back(std::stoi(argv[++i]));
        }
    }

    // Optional: replace the computed precision-200 vertex grid with one
    // captured from the running game, so the engine-native Vector3.Slerp stops
    // being a variable in the comparison.
    if (!vertexGridPath.empty()) {
        std::ifstream file(vertexGridPath, std::ios::binary);
        if (!file) {
            std::cerr << "cannot open vertex grid " << vertexGridPath << '\n';
            return 2;
        }
        file.seekg(0, std::ios::end);
        const auto size = static_cast<std::size_t>(file.tellg());
        file.seekg(0);
        std::vector<dspugen::terrain::Vector3> grid(size / sizeof(dspugen::terrain::Vector3));
        file.read(reinterpret_cast<char *>(grid.data()),
                  static_cast<std::streamsize>(size));
        if (!dspugen::terrain::PlanetRawData::installVertexGrid(200, grid)) {
            std::cerr << "vertex grid length does not match precision 200\n";
            return 2;
        }
        std::cerr << "installed captured precision-200 vertex grid ("
                  << grid.size() << " vertices)\n";
    }

    dspugen::loadProtoSets();
    dspugen::Galaxy::initThread();
    dspugen::Star::initThread();
    dspugen::Planet::initThread();

    if (metaFrom >= 0) {
        std::cout << "# dsp-meta-parity v1\n";
        for (int seed = metaFrom; seed <= metaTo; ++seed)
            dumpMeta(seed, metaStars);
    } else if (!heightDump.empty()) {
        dumpHeights(heightSeed, 64, heightDump);
    } else if (parity) {
        std::cout << "# dsp-vein-parity v1\n";
        for (int seed = parityFrom; seed <= parityTo; ++seed)
            dumpGalaxyParity(seed, 64);
    } else {
        dumpGalaxy(0, 64, fullSummary);
        dumpGalaxy(1, 64, fullSummary);
    }

    dspugen::Planet::releaseThread();
    dspugen::Star::releaseThread();
    dspugen::Galaxy::releaseThread();
    return 0;
}
