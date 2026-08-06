#pragma once

#include "unity_math.hh"

#include <cstdint>
#include <vector>

namespace dspugen::terrain {

class PlanetRawData {
public:
    explicit PlanetRawData(int precision);

    [[nodiscard]] int precision() const { return precisionValue; }
    // Unity exposes this as a precision-derived property, not the current
    // backing-array length. Keep it stable even while a caller fills a buffer.
    [[nodiscard]] int dataLength() const {
        return (precisionValue + 1) * (precisionValue + 1) * 4;
    }
    [[nodiscard]] int stride() const { return (precisionValue + 1) * 2; }
    [[nodiscard]] int substride() const { return precisionValue + 1; }

    // Supplies a vertex grid captured from the running game for one precision.
    // Vector3.Slerp lives in Unity's engine native code, so calcVerts() cannot
    // reproduce it bit for bit; when a captured grid is installed it is used
    // verbatim and only the index map is recomputed from it.  Returns false if
    // the data length does not match the precision.  Global and immutable once
    // set: install it before starting worker threads.
    static bool installVertexGrid(int precision, const std::vector<Vector3> &vertices);
    [[nodiscard]] static bool hasVertexGrid(int precision);

    // Fills the vertex grid and index map, reusing a thread-local per-precision
    // copy the way the reference reuses its verts200/indexMap200 arrays.
    void calcVerts();
    // Clears the per-planet buffers, leaving the precision-derived vertex grid
    // and index map in place so one instance can be reused across planets.
    void clearTerrain();

    [[nodiscard]] const std::vector<Vector3> &vertices() const { return vertexData; }
    [[nodiscard]] std::vector<Vector3> &vertices() { return vertexData; }
    [[nodiscard]] const std::vector<std::uint16_t> &heightData() const { return heightValues; }
    [[nodiscard]] std::vector<std::uint16_t> &heightData() { return heightValues; }
    [[nodiscard]] const std::vector<std::uint8_t> &biomoData() const { return biomoValues; }
    [[nodiscard]] std::vector<std::uint8_t> &biomoData() { return biomoValues; }

    [[nodiscard]] int positionHash(Vector3 value, int corner = 0) const;
    [[nodiscard]] int queryIndex(Vector3 value) const;
    // Starts a per-planet lazy terrain pass. queryHeight evaluates and caches
    // only the grid vertices its interpolation window actually touches.
    void beginLazyTerrain(int algorithmId, int seed, float radius,
                          double modX, double modY);
    void endLazyTerrain();
    [[nodiscard]] float queryHeight(Vector3 value);
    // Deliberately empty. The reference clears vegetable cells under a vein
    // node, which affects neither queryHeight nor the water rejection test, so
    // there is nothing for the amount summary to track. The call is kept at its
    // original position so the reference sequence stays readable.
    void eraseVegetableAtPoint(Vector3) {}

private:
    void calcVertsUncached();
    void rebuildIndexMap();
    void ensureLazyHeights(const int *indices, std::size_t count);
    [[nodiscard]] int trans(float value, int precision) const;

    int precisionValue;
    int indexMapPrecision;
    int indexMapFaceStride;
    int indexMapCornerStride;
    std::vector<std::uint16_t> heightValues;
    std::vector<std::uint8_t> biomoValues;
    std::vector<Vector3> vertexData;
    std::vector<int> indexMap;
    std::vector<std::uint32_t> heightGeneration;
    std::uint32_t currentHeightGeneration = 0;
    bool lazyTerrainActive = false;
    int lazyAlgorithmId = 0;
    int lazySeed = 0;
    float lazyRadius = 0.0f;
    double lazyModX = 0.0;
    double lazyModY = 0.0;
};

}
