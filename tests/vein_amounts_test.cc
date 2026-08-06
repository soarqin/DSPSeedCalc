#include "galaxy.hh"
#include "planet.hh"
#include "protoset.hh"
#include "settings.hh"
#include "terrain/planet_algorithm.hh"
#include "terrain/planet_geometry.hh"
#include "terrain/planet_raw_data.hh"
#include "terrain/simplex_noise.hh"
#include "util/maths.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

static std::uint64_t doubleBits(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool nearFloat(float lhs, float rhs, float epsilon = 1.0E-05f) {
    return std::abs(lhs - rhs) <= epsilon;
}

static bool sameVertices(const std::vector<dspugen::terrain::Vector3> &lhs,
                         const std::vector<dspugen::terrain::Vector3> &rhs) {
    return lhs.size() == rhs.size() &&
           std::memcmp(lhs.data(), rhs.data(),
                       lhs.size() * sizeof(dspugen::terrain::Vector3)) == 0;
}

static void testUnitySlerpContract() {
    using dspugen::terrain::Vector3;
    using dspugen::terrain::slerp;

    Vector3 a{1.0f, 0.0f, 0.0f};
    Vector3 b{0.0f, 1.0f, 0.0f};
    auto below = slerp(a, b, -0.25f);
    auto above = slerp(a, b, 1.25f);
    assert(nearFloat(below.x, a.x));
    assert(nearFloat(below.y, a.y));
    assert(nearFloat(above.x, b.x));
    assert(nearFloat(above.y, b.y));

    auto antiparallel = slerp(a, Vector3{-1.0f, 0.0f, 0.0f}, 0.5f);
    assert(nearFloat(antiparallel.x, 0.0f));
    assert(nearFloat(antiparallel.magnitude(), 1.0f));

    auto nonUnit = slerp(Vector3{2.0f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 3.0f}, 0.5f);
    constexpr float expected = 1.76776695f;
    assert(nearFloat(nonUnit.x, expected));
    assert(nearFloat(nonUnit.z, expected));
    assert(nearFloat(nonUnit.magnitude(), 2.5f));
}

static void testRawDataContract() {
    assert(dspugen::util::mathfRoundToInt(0.5f) == 0);
    assert(dspugen::util::mathfRoundToInt(1.5f) == 2);
    assert(dspugen::util::mathfRoundToInt(-0.5f) == 0);
    assert(dspugen::util::mathfRoundToInt(100.0f * 0.5f * 1.1f) == 55);
    assert(dspugen::util::mathfRoundToInt(1.0f * 0.5f * 1.1f) == 1);

    dspugen::terrain::PlanetRawData raw(200);
    assert(raw.dataLength() == 4 * 201 * 201);
    raw.calcVerts();
    assert(raw.vertices().size() == static_cast<std::size_t>(raw.dataLength()));
    for (auto &height : raw.heightData()) height = 1234;
    assert(nearFloat(raw.queryHeight({0.0f, 1.0f, 0.0f}), 12.34f));
    raw.heightData().resize(1);
    assert(raw.dataLength() == 4 * 201 * 201);

    // calcVerts() is served from a thread-local per-precision cache, so a second
    // instance must come out bit-identical to a freshly computed one.
    dspugen::terrain::PlanetRawData cached(200);
    cached.calcVerts();
    assert(sameVertices(cached.vertices(), raw.vertices()));
    for (auto value : cached.heightData()) assert(value == 0);
    cached.heightData()[7] = 4321;
    cached.clearTerrain();
    assert(cached.heightData()[7] == 0);
    assert(sameVertices(cached.vertices(), raw.vertices()));
}

static void testSimplexNoiseFixture() {
    // Values captured from the recorded Mono SimplexNoise implementation
    // (game build 0.10.34.28529, seed 12345).
    dspugen::terrain::SimplexNoise noise(12345);
    constexpr double x = 0.125;
    constexpr double y = -0.75;
    constexpr double z = 1.5;
    assert(doubleBits(noise.Noise(x)) == doubleBits(-0.37928771972655123));
    assert(doubleBits(noise.Noise(x, y)) == doubleBits(0.15774472974548398));
    assert(doubleBits(noise.Noise(x, y, z)) == doubleBits(-0.3432531111169672));
    assert(doubleBits(noise.Noise3DFBM(x, y, z, 4)) ==
           doubleBits(-0.3620849029036741));
    assert(doubleBits(noise.RidgedNoise(x, y, z, 4)) ==
           doubleBits(0.3620849029036741));
    assert(doubleBits(noise.Noise(x, y, z, -0.25)) == 0xBF9FFC383247BBA3ULL);
    assert(doubleBits(noise.Noise2DFBM(x, y, 4)) == 0xBFC1D52C010F92DDULL);
    assert(doubleBits(noise.Noise3DFBMInitialAmp(x, y, z, 4, 0.6, 2.0, 0.7)) ==
           0xBFE259EF9E483E70ULL);
    assert(doubleBits(noise.RidgedNoise(x, y, z, 4, 0.7, 2.0, 0.8)) ==
           0x3FE79BA011FEADF2ULL);
}

static void testTerrainAlgorithmDispatch() {
    using dspugen::terrain::PlanetAlgorithmContext;
    using dspugen::terrain::PlanetRawData;
    using dspugen::terrain::TerrainGenerationResult;
    using dspugen::terrain::dispatchTerrain;

    PlanetRawData raw(200);
    raw.calcVerts();
    PlanetRawData lazy(200);
    lazy.calcVerts();
    PlanetAlgorithmContext context{raw, 12345, 200.0f};
    const std::array<int, 5> sampleIndices{
        0, 1, 12345, 80000, raw.dataLength() - 1};
    const auto assertLazyMatches = [&](int algorithm, double modX,
                                       double modY) {
        lazy.beginLazyTerrain(algorithm, 12345, 200.0f, modX, modY);
        for (int index: sampleIndices) {
            auto direction = raw.vertices()[static_cast<std::size_t>(index)];
            auto expected = raw.queryHeight(direction);
            auto actual = lazy.queryHeight(direction);
            assert(std::memcmp(&actual, &expected, sizeof(float)) == 0);
        }
        lazy.endLazyTerrain();
    };

    auto result = dispatchTerrain(0, context, 1.0, 0.0);
    assert(result == TerrainGenerationResult::Generated);
    assert(raw.heightData()[0] == 20000);
    assert(raw.biomoData()[0] == 0);
    assertLazyMatches(0, 1.0, 0.0);

    result = dispatchTerrain(8, context, 1.0, 0.0);
    assert(result == TerrainGenerationResult::Generated);
    assertLazyMatches(8, 1.0, 0.0);
    bool varied = false;
    for (auto height : raw.heightData()) {
        if (height != 20000) {
            varied = true;
            break;
        }
    }
    assert(varied);

    for (int algorithm = 1; algorithm <= 6; ++algorithm) {
        result = dispatchTerrain(algorithm, context, 0.8, -0.2);
        assert(result == TerrainGenerationResult::Generated);
        assertLazyMatches(algorithm, 0.8, -0.2);
        bool populated = false;
        for (auto height : raw.heightData()) {
            if (height != 0) {
                populated = true;
                break;
            }
        }
        assert(populated);
    }
    for (int algorithm = 7; algorithm <= 13; ++algorithm) {
        result = dispatchTerrain(algorithm, context, 1.0, 0.0);
        assert(result == TerrainGenerationResult::Generated);
        assertLazyMatches(algorithm, 1.0, 0.0);
        bool populated = false;
        for (auto height : raw.heightData()) {
            if (height != 0) {
                populated = true;
                break;
            }
        }
        assert(populated);
    }
    std::fill(raw.heightData().begin(), raw.heightData().end(), 0);
    std::fill(raw.biomoData().begin(), raw.biomoData().end(), 0);
    result = dispatchTerrain(14, context, 1.0, 0.0);
    assert(result == TerrainGenerationResult::Generated);
    assertLazyMatches(14, 1.0, 0.0);
    bool algorithm14Populated = false;
    for (auto height : raw.heightData()) {
        if (height != 0) {
            algorithm14Populated = true;
            break;
        }
    }
    assert(algorithm14Populated);

    result = dispatchTerrain(15, context, 1.0, 0.0);
    assert(result == TerrainGenerationResult::UnsupportedAlgorithm);
}

static void testVeinPlacementRules() {
    using dspugen::EVeinType;
    using dspugen::vein_detail::acceptsVeinCenter;
    using dspugen::vein_detail::acceptsVeinNode;

    constexpr float radius = 200.0f;
    assert(!acceptsVeinCenter(1, EVeinType::Oil, radius + 0.49f, radius,
                             radius));
    assert(acceptsVeinCenter(1, EVeinType::Oil, radius + 0.5f, radius,
                            radius));
    assert(acceptsVeinCenter(11, EVeinType::Iron, radius + 0.7f, radius,
                            radius));
    assert(!acceptsVeinCenter(11, EVeinType::Iron, radius + 0.7001f, radius,
                             radius));
    assert(!acceptsVeinCenter(11, EVeinType::Silicium, radius + 0.7f, radius,
                             radius));
    assert(acceptsVeinCenter(11, EVeinType::Silicium, radius + 0.7001f, radius,
                            radius));
    assert(!acceptsVeinCenter(12, EVeinType::Fireice, radius + 1.1999f,
                             radius, radius));
    assert(acceptsVeinCenter(12, EVeinType::Fireice, radius + 1.2f,
                            radius, radius));
    assert(acceptsVeinCenter(13, EVeinType::Titanium, radius + 0.7f,
                            radius, radius));
    assert(!acceptsVeinCenter(13, EVeinType::Titanium, radius + 0.7001f,
                             radius, radius));
    assert(acceptsVeinCenter(7, EVeinType::Bamboo, radius - 4.0f, radius,
                            radius));
    assert(!acceptsVeinCenter(7, EVeinType::Bamboo, radius - 3.9999f, radius,
                             radius));
    assert(acceptsVeinCenter(7, EVeinType::Oil, radius - 50.0f, radius,
                            radius));

    assert(acceptsVeinNode(1, 0, radius - 1.0f, radius));
    assert(!acceptsVeinNode(1, 1000, radius - 1.0f, radius));
    assert(acceptsVeinNode(1, 1000, radius, radius));
    assert(acceptsVeinNode(7, 1000, radius - 50.0f, radius));
}

int main() {
    testUnitySlerpContract();
    testRawDataContract();
    testSimplexNoiseFixture();
    testTerrainAlgorithmDispatch();
    testVeinPlacementRules();
    dspugen::loadProtoSets();
    dspugen::settings = {};
    dspugen::settings.hasPlanets = true;
    dspugen::Galaxy::initThread();
    dspugen::Star::initThread();
    dspugen::Planet::initThread();
    auto *algorithm14Galaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, 0, 64);
    assert(algorithm14Galaxy != nullptr);
    auto *algorithm14Planet = algorithm14Galaxy->stars[0]->planets[1];
    algorithm14Planet->algoId = 14;
    assert(algorithm14Planet->ensureVeinAmounts() ==
           dspugen::VeinAmountGenerationResult::Generated);
    assert(algorithm14Planet->veinAmountSummary() != nullptr);
    algorithm14Galaxy->release();

    auto *unsupportedGalaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, 0, 64);
    assert(unsupportedGalaxy != nullptr);
    auto *unsupportedPlanet = unsupportedGalaxy->stars[0]->planets[1];
    unsupportedPlanet->algoId = 15;
    assert(unsupportedPlanet->ensureVeinAmounts() ==
           dspugen::VeinAmountGenerationResult::UnsupportedAlgorithm);
    assert(unsupportedPlanet->veinAmountSummary() == nullptr);
    // The galaxy pass reports the first failure instead of throwing: it is
    // reachable through the plugin C ABI.
    const auto batch = unsupportedGalaxy->ensureAllVeinAmounts();
    assert(!batch.ok());
    assert(batch.result == dspugen::VeinAmountGenerationResult::UnsupportedAlgorithm);
    assert(batch.planetId == unsupportedPlanet->id);
    assert(batch.algoId == 15);
    assert(std::string(dspugen::veinAmountResultName(batch.result)) ==
           "unsupported algorithm");
    unsupportedGalaxy->release();

    auto *gasGalaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, 0, 64);
    assert(gasGalaxy != nullptr);
    dspugen::Planet *gasPlanet = nullptr;
    for (auto *star: gasGalaxy->stars) {
        for (auto *planet: star->planets) {
            if (planet->type == dspugen::EPlanetType::Gas) {
                gasPlanet = planet;
                break;
            }
        }
        if (gasPlanet) break;
    }
    assert(gasPlanet != nullptr);
    assert(gasPlanet->veinAmountGenerationResult() ==
           dspugen::VeinAmountGenerationResult::NotGenerated);
    assert(gasPlanet->ensureVeinAmounts() ==
           dspugen::VeinAmountGenerationResult::Generated);
    const auto *gasSummary = gasPlanet->veinAmountSummary();
    assert(gasSummary != nullptr);
    for (const auto count: gasSummary->nodeCount) assert(count == 0);
    for (const auto amount: gasSummary->baseAmount) assert(amount == 0);
    assert(gasPlanet->terrainDigest().height == 0);
    assert(gasPlanet->terrainDigest().biomo == 0);
    assert(gasPlanet->ensureVeinAmounts() ==
           dspugen::VeinAmountGenerationResult::Generated);
    assert(gasPlanet->veinAmountSummary() == gasSummary);
    gasGalaxy->release();

    // A per-planet request must leave every rejected planet on the cheap path.
    // Compare its result with a separate whole-galaxy pass below so the cache
    // cannot hide an ordering dependency.
    auto *singlePlanetGalaxy = dspugen::Galaxy::create(
        dspugen::DefaultAlgoVersion, 0, 64);
    assert(singlePlanetGalaxy != nullptr);
    const int requestedPlanetId = singlePlanetGalaxy->birthPlanetId;
    auto *requestedPlanet = singlePlanetGalaxy->planetById(requestedPlanetId);
    assert(requestedPlanet != nullptr);
    for (const auto *star: singlePlanetGalaxy->stars)
        for (const auto *planet: star->planets)
            assert(planet->veinAmountSummary() == nullptr);
    assert(requestedPlanet->ensureVeinAmounts() ==
           dspugen::VeinAmountGenerationResult::Generated);
    const auto *requestedSummary = requestedPlanet->veinAmountSummary();
    assert(requestedSummary != nullptr);
    const auto expectedRequestedSummary = *requestedSummary;
    int generatedPlanetCount = 0;
    for (const auto *star: singlePlanetGalaxy->stars) {
        for (const auto *planet: star->planets) {
            if (planet->veinAmountSummary()) ++generatedPlanetCount;
        }
    }
    assert(generatedPlanetCount == 1);
    assert(requestedPlanet->ensureVeinAmounts() ==
           dspugen::VeinAmountGenerationResult::Generated);
    assert(requestedPlanet->veinAmountSummary() == requestedSummary);
    singlePlanetGalaxy->release();

    auto *galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, 0, 64);
    assert(galaxy != nullptr);
    int discoveredBirthPlanetId = 0;
    for (const auto *planet: galaxy->stars[0]->planets) {
        const auto *theme = dspugen::themeProtoSet.select(planet->theme);
        if (theme != nullptr &&
            theme->distribute == dspugen::EThemeDistribute::Birth) {
            discoveredBirthPlanetId = planet->id;
            break;
        }
    }
    assert(discoveredBirthPlanetId != 0);
    assert(galaxy->birthPlanetId == discoveredBirthPlanetId);
    const auto *firstPlanet = galaxy->stars[0]->planets[0];
    const auto *firstTheme = dspugen::themeProtoSet.select(firstPlanet->theme);
    assert(firstTheme != nullptr);
    assert(firstPlanet->modX >= static_cast<double>(firstTheme->modX.x));
    assert(firstPlanet->modX <= static_cast<double>(firstTheme->modX.y));
    assert(firstPlanet->modY >= static_cast<double>(firstTheme->modY.x));
    assert(firstPlanet->modY <= static_cast<double>(firstTheme->modY.y));
    assert(firstPlanet->waterItemId == firstTheme->waterItemId);
    assert(firstPlanet->orbitalPeriod > 0.0);
    assert(firstPlanet->rotationPeriod != 0.0);
    const auto pose = dspugen::terrain::predictPose(*firstPlanet, 85.0);
    assert(nearFloat(pose.rotation.sqrMagnitude(), 1.0f, 2.0E-05f));
    const auto localStarDirection =
        dspugen::terrain::predictLocalStarDirection(*firstPlanet, 85.0);
    assert(nearFloat(localStarDirection.magnitude(), 1.0f));

    dspugen::terrain::PlanetRawData birthRaw(200);
    birthRaw.calcVerts();
    dspugen::terrain::PlanetAlgorithmContext birthTerrain{
        birthRaw, firstPlanet->seed, firstPlanet->radius};
    assert(dspugen::terrain::dispatchTerrain(
               firstPlanet->algoId, birthTerrain,
               firstPlanet->modX, firstPlanet->modY) ==
           dspugen::terrain::TerrainGenerationResult::Generated);
    const auto birthPoints =
        dspugen::terrain::generateBirthPoints(*firstPlanet, birthRaw);
    assert(birthPoints.birthPoint.magnitude() > firstPlanet->realRadius());
    assert(nearFloat(birthPoints.resourcePoint0.magnitude(), 1.0f));
    assert(nearFloat(birthPoints.resourcePoint1.magnitude(), 1.0f));

    const dspugen::terrain::Vector3 unsnapped{0.371f, 0.812f, -0.451f};
    const auto snapped = dspugen::terrain::rawSnap(unsnapped, firstPlanet->radius);
    const auto snappedAgain = dspugen::terrain::rawSnap(snapped, firstPlanet->radius);
    assert(nearFloat(snapped.magnitude(), 1.0f));
    assert(nearFloat((snappedAgain - snapped).magnitude(), 0.0f, 1.0E-06f));
    assert(galaxy->stars[0]->planets[0]->veinAmountSummary() == nullptr);
    assert(galaxy->ensureAllVeinAmounts().ok());
    const auto *fullRequestedPlanet = galaxy->planetById(requestedPlanetId);
    assert(fullRequestedPlanet != nullptr);
    const auto *fullRequestedSummary = fullRequestedPlanet->veinAmountSummary();
    assert(fullRequestedSummary != nullptr);
    assert(fullRequestedSummary->nodeCount == expectedRequestedSummary.nodeCount);
    assert(fullRequestedSummary->baseAmount == expectedRequestedSummary.baseAmount);
    auto *summary = galaxy->stars[0]->planets[0]->veinAmountSummary();
    assert(summary != nullptr);
    assert(summary->nodeCount[0] == 0);
    bool sawOrdinaryPlanet = false;
    bool sawGeneratedNode = false;
    for (const auto *star: galaxy->stars) {
        if (star == nullptr) continue;
        for (const auto *planet: star->planets) {
            if (planet == nullptr || planet->type == dspugen::EPlanetType::Gas ||
                planet->algoId == 0 || planet->algoId == 7 || planet->algoId == 11 ||
                planet->algoId == 12 || planet->algoId == 13)
                continue;
            sawOrdinaryPlanet = true;
            const auto *planetSummary = planet->veinAmountSummary();
            assert(planetSummary != nullptr);
            for (auto count: planetSummary->nodeCount)
                if (count != 0) sawGeneratedNode = true;
        }
    }
    assert(sawOrdinaryPlanet);
    assert(sawGeneratedNode);
    auto firstSummary = *summary;
    assert(galaxy->ensureAllVeinAmounts().ok());
    assert(galaxy->stars[0]->planets[0]->veinAmountSummary() == summary);
    assert(galaxy->stars[0]->planets[0]->veinAmountSummary()->nodeCount ==
           firstSummary.nodeCount);
    galaxy->release();

    auto *secondGalaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, 1, 64);
    assert(secondGalaxy != nullptr);
    assert(secondGalaxy->stars[0]->planets[0]->veinAmountSummary() == nullptr);
    assert(secondGalaxy->ensureAllVeinAmounts().ok());
    assert(secondGalaxy->stars[0]->planets[0]->veinAmountSummary() != nullptr);
    secondGalaxy->release();
    dspugen::Planet::releaseThread();
    dspugen::Star::releaseThread();
    dspugen::Galaxy::releaseThread();
}
