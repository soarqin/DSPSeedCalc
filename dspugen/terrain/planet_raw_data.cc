#include "planet_raw_data.hh"

#include "planet_algorithm.hh"
#include "../util/instrumentation.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <limits>

namespace dspugen::terrain {

namespace {

constexpr std::array<Vector3, 6> poles = {{
    {1.0f, 0.0f, 0.0f},
    {-1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, -1.0f},
}};

struct CachedGrid {
    int precision;
    std::vector<Vector3> vertices;
    std::vector<int> indexMap;
};

std::vector<CachedGrid> &calcVertsCache() {
    static thread_local std::vector<CachedGrid> cache;
    return cache;
}

// Captured grids, keyed by precision.  Written once before threads start and
// read-only afterwards, so no synchronisation is needed.
std::map<int, std::vector<Vector3>> &installedGrids() {
    static std::map<int, std::vector<Vector3>> grids;
    return grids;
}

}

bool PlanetRawData::installVertexGrid(int precision,
                                      const std::vector<Vector3> &vertices) {
    const auto expected =
        static_cast<std::size_t>((precision + 1) * (precision + 1) * 4);
    if (vertices.size() != expected) return false;
    installedGrids()[precision] = vertices;
    calcVertsCache().clear();
    return true;
}

bool PlanetRawData::hasVertexGrid(int precision) {
    return installedGrids().count(precision) != 0;
}

PlanetRawData::PlanetRawData(int precision)
    : precisionValue(precision),
      indexMapPrecision(precision >> 2),
      indexMapFaceStride(indexMapPrecision * indexMapPrecision),
      indexMapCornerStride(indexMapFaceStride * 3),
      heightValues(static_cast<std::size_t>((precision + 1) * (precision + 1) * 4)),
      biomoValues(heightValues.size()),
      vertexData(heightValues.size()),
      // Array<int> in the reference constructor is zero-initialized; CalcVerts
      // applies the -1 sentinel before it populates the map.
      indexMap(static_cast<std::size_t>(indexMapCornerStride * 8)),
      heightGeneration(heightValues.size()) {}

void PlanetRawData::clearTerrain() {
    std::fill(heightValues.begin(), heightValues.end(), 0);
    std::fill(biomoValues.begin(), biomoValues.end(), 0);
}

void PlanetRawData::calcVerts() {
    // The vertex grid and its index map depend only on the precision, so the
    // reference computes them once and copies them into every later instance
    // (PlanetRawData.verts200 / indexMap200).  Recomputing per planet costs
    // about 20 ms and roughly 500k Slerp calls for nothing.  The cache is
    // thread-local, matching the Galaxy/Star/Planet pools, and holds one entry
    // per precision actually used.
    auto &cache = calcVertsCache();
    for (const auto &entry: cache) {
        if (entry.precision != precisionValue) continue;
        vertexData = entry.vertices;
        indexMap = entry.indexMap;
        return;
    }

    const auto &grids = installedGrids();
    const auto installed = grids.find(precisionValue);
    if (installed != grids.end()) {
        vertexData = installed->second;
        rebuildIndexMap();
    } else {
        calcVertsUncached();
    }
    cache.push_back({precisionValue, vertexData, indexMap});
}

// The index map is a pure function of the vertex grid, so it is rebuilt rather
// than captured.
void PlanetRawData::rebuildIndexMap() {
    std::fill(indexMap.begin(), indexMap.end(), -1);
    const auto faceStride = (precisionValue + 1) * 2;
    const auto faceSize = precisionValue + 1;
    for (int j = 0; j < dataLength(); ++j) {
        const auto num3 = j % faceStride;
        const auto num4 = j / faceStride;
        const auto num5 = num3 % faceSize;
        const auto num6 = num4 % faceSize;
        const auto num7 = (((num3 >= faceSize) ? 1 : 0) +
                           ((num4 >= faceSize) ? 1 : 0) * 2) * 2 +
                          ((num5 < num6) ? 1 : 0);
        static constexpr int corners[8] = {7, 5, 6, 4, 2, 0, 3, 1};
        const auto hash = positionHash(vertexData[static_cast<std::size_t>(j)],
                                       corners[num7]);
        if (indexMap[static_cast<std::size_t>(hash)] == -1)
            indexMap[static_cast<std::size_t>(hash)] = j;
    }
    for (std::size_t i = 1; i < indexMap.size(); ++i)
        if (indexMap[i] == -1) indexMap[i] = indexMap[i - 1];
}

void PlanetRawData::calcVertsUncached() {
    std::fill(indexMap.begin(), indexMap.end(), -1);
    auto faceStride = (precisionValue + 1) * 2;
    auto faceSize = precisionValue + 1;
    for (int j = 0; j < dataLength(); ++j) {
        auto num3 = j % faceStride;
        auto num4 = j / faceStride;
        auto num5 = num3 % faceSize;
        auto num6 = num4 % faceSize;
        auto num7 = (((num3 >= faceSize) ? 1 : 0) +
                     ((num4 >= faceSize) ? 1 : 0) * 2) * 2 +
                    ((num5 < num6) ? 1 : 0);
        auto num8 = static_cast<float>((num5 >= num6) ? precisionValue - num5 : num5);
        auto num9 = static_cast<float>((num5 >= num6) ? num6 : precisionValue - num6);
        auto num10 = static_cast<float>(precisionValue) - num9;
        num9 /= static_cast<float>(precisionValue);
        num8 = (num10 > 0.0f) ? num8 / num10 : 0.0f;
        Vector3 a;
        Vector3 a2;
        Vector3 b;
        int corner;
        switch (num7) {
        case 0: a = poles[2]; a2 = poles[0]; b = poles[4]; corner = 7; break;
        case 1: a = poles[3]; a2 = poles[4]; b = poles[0]; corner = 5; break;
        case 2: a = poles[2]; a2 = poles[4]; b = poles[1]; corner = 6; break;
        case 3: a = poles[3]; a2 = poles[1]; b = poles[4]; corner = 4; break;
        case 4: a = poles[2]; a2 = poles[1]; b = poles[5]; corner = 2; break;
        case 5: a = poles[3]; a2 = poles[5]; b = poles[1]; corner = 0; break;
        case 6: a = poles[2]; a2 = poles[5]; b = poles[0]; corner = 3; break;
        case 7: a = poles[3]; a2 = poles[0]; b = poles[5]; corner = 1; break;
        default: a = poles[2]; a2 = poles[0]; b = poles[4]; corner = 7; break;
        }
        vertexData[static_cast<std::size_t>(j)] =
            slerp(slerp(a, b, num9), slerp(a2, b, num9), num8);
        auto hash = positionHash(vertexData[static_cast<std::size_t>(j)], corner);
        if (indexMap[static_cast<std::size_t>(hash)] == -1)
            indexMap[static_cast<std::size_t>(hash)] = j;
    }
    for (std::size_t i = 1; i < indexMap.size(); ++i)
        if (indexMap[i] == -1) indexMap[i] = indexMap[i - 1];
}

int PlanetRawData::trans(float value, int precision) const {
    auto result = static_cast<int>((std::sqrt(value + 0.23f) - 0.4795832f) /
                                   0.6294705f * static_cast<float>(precision));
    if (result >= precision) result = precision - 1;
    return result;
}

int PlanetRawData::positionHash(Vector3 value, int corner) const {
    if (corner == 0)
        corner = (value.x > 0.0f ? 1 : 0) + (value.y > 0.0f ? 2 : 0) +
                 (value.z > 0.0f ? 4 : 0);
    if (value.x < 0.0f) value.x = 0.0f - value.x;
    if (value.y < 0.0f) value.y = 0.0f - value.y;
    if (value.z < 0.0f) value.z = 0.0f - value.z;
    if (static_cast<double>(value.x) < 1.0E-06 &&
        static_cast<double>(value.y) < 1.0E-06 &&
        static_cast<double>(value.z) < 1.0E-06)
        return 0;
    int face;
    int u;
    int v;
    if (value.x >= value.y && value.x >= value.z) {
        face = 0;
        u = trans(value.z / value.x, indexMapPrecision);
        v = trans(value.y / value.x, indexMapPrecision);
    } else if (value.y >= value.x && value.y >= value.z) {
        face = 1;
        u = trans(value.x / value.y, indexMapPrecision);
        v = trans(value.z / value.y, indexMapPrecision);
    } else {
        face = 2;
        u = trans(value.x / value.z, indexMapPrecision);
        v = trans(value.y / value.z, indexMapPrecision);
    }
    return u + v * indexMapPrecision + face * indexMapFaceStride +
           corner * indexMapCornerStride;
}

int PlanetRawData::queryIndex(Vector3 value) const {
    value.normalize();
    auto hash = positionHash(value);
    auto start = indexMap[static_cast<std::size_t>(hash)];
    auto radius = MathfPi / static_cast<float>(precisionValue * 2) * 0.25f;
    auto radiusSquared = radius * radius;
    auto result = start;
    auto best = 10.0f;
    auto rowStride = stride();
    for (int i = -1; i <= 3; ++i) {
        for (int j = -1; j <= 3; ++j) {
            auto index = start + i + j * rowStride;
            if (index < 0 || index >= dataLength()) continue;
            auto distance = (vertexData[static_cast<std::size_t>(index)] - value).sqrMagnitude();
            if (distance < radiusSquared) return index;
            if (distance < best) {
                best = distance;
                result = index;
            }
        }
    }
    return result;
}

void PlanetRawData::beginLazyTerrain(int algorithmId, int seed, float radius,
                                     double modX, double modY) {
    if (++currentHeightGeneration == 0) {
        std::fill(heightGeneration.begin(), heightGeneration.end(), 0);
        currentHeightGeneration = 1;
    }
    lazyTerrainActive = true;
    lazyAlgorithmId = algorithmId;
    lazySeed = seed;
    lazyRadius = radius;
    lazyModX = modX;
    lazyModY = modY;
}

void PlanetRawData::endLazyTerrain() {
    lazyTerrainActive = false;
}

void PlanetRawData::ensureLazyHeights(const int *indices, std::size_t count) {
    if (!lazyTerrainActive || count == 0) return;
    DSP_INSTRUMENT(
        ++util::instrumentation::local().terrainSparseBatches;
        util::instrumentation::local().terrainVertexEvaluations += count;
    );
    PlanetAlgorithmContext context{
        *this, lazySeed, lazyRadius, false, indices, count};
    (void)dispatchTerrain(lazyAlgorithmId, context, lazyModX, lazyModY);
    for (std::size_t i = 0; i < count; ++i)
        heightGeneration[static_cast<std::size_t>(indices[i])] =
            currentHeightGeneration;
}

float PlanetRawData::queryHeight(Vector3 value) {
    DSP_INSTRUMENT(++util::instrumentation::local().terrainHeightQueries);
    value.normalize();
    auto hash = positionHash(value);
    auto start = indexMap[static_cast<std::size_t>(hash)];
    auto radius = MathfPi / static_cast<float>(precisionValue * 2) * 1.2f;
    auto radiusSquared = radius * radius;
    auto rowStride = stride();
    std::array<int, 25> missing{};
    std::array<int, 25> weightedIndices{};
    std::array<float, 25> weightedDistances{};
    std::size_t missingCount = 0;
    std::size_t weightedCount = 0;
    for (int i = -1; i <= 3; ++i) {
        for (int j = -1; j <= 3; ++j) {
            auto index = start + i + j * rowStride;
            if (index < 0 || index >= dataLength()) continue;
            auto distance =
                (vertexData[static_cast<std::size_t>(index)] - value)
                    .sqrMagnitude();
            if (distance > radiusSquared) continue;
            weightedIndices[weightedCount] = index;
            weightedDistances[weightedCount] = distance;
            ++weightedCount;
            if (lazyTerrainActive &&
                heightGeneration[static_cast<std::size_t>(index)] !=
                    currentHeightGeneration)
                missing[missingCount++] = index;
        }
    }
    if (lazyTerrainActive && weightedCount == 0 &&
        heightGeneration[0] != currentHeightGeneration)
        missing[missingCount++] = 0;
    ensureLazyHeights(missing.data(), missingCount);

    float weightSum = 0.0f;
    float heightSum = 0.0f;
    for (std::size_t i = 0; i < weightedCount; ++i) {
        const auto index = weightedIndices[i];
        const auto distance = weightedDistances[i];
        // Each managed statement rounds once: the expression itself is
        // evaluated in double.
        auto weight = static_cast<float>(
            1.0 - static_cast<double>(static_cast<float>(std::sqrt(
                      static_cast<double>(distance)))) /
                      static_cast<double>(radius));
        auto height = static_cast<float>(
            static_cast<int>(heightValues[static_cast<std::size_t>(index)]));
        weightSum = static_cast<float>(static_cast<double>(weightSum) +
                                       static_cast<double>(weight));
        heightSum = static_cast<float>(static_cast<double>(heightSum) +
                                       static_cast<double>(height) *
                                           static_cast<double>(weight));
    }
    if (weightSum == 0.0f)
        return static_cast<float>(static_cast<double>(static_cast<int>(heightValues[0])) *
                                  static_cast<double>(0.01f));
    return static_cast<float>(static_cast<double>(heightSum) /
                              static_cast<double>(weightSum) *
                              static_cast<double>(0.01f));
}

}
