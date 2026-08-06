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

    void initModData();
    void calcVerts();

    [[nodiscard]] const std::vector<Vector3> &vertices() const { return vertexData; }
    [[nodiscard]] std::vector<Vector3> &vertices() { return vertexData; }
    [[nodiscard]] const std::vector<std::uint16_t> &heightData() const { return heightValues; }
    [[nodiscard]] std::vector<std::uint16_t> &heightData() { return heightValues; }
    [[nodiscard]] const std::vector<std::uint8_t> &biomoData() const { return biomoValues; }
    [[nodiscard]] std::vector<std::uint8_t> &biomoData() { return biomoValues; }

    [[nodiscard]] int positionHash(Vector3 value, int corner = 0) const;
    [[nodiscard]] int queryIndex(Vector3 value) const;
    [[nodiscard]] float queryHeight(Vector3 value) const;
    void eraseVegetableAtPoint(Vector3) {}

private:
    [[nodiscard]] int trans(float value, int precision) const;

    int precisionValue;
    int indexMapPrecision;
    int indexMapFaceStride;
    int indexMapCornerStride;
    std::vector<std::uint16_t> heightValues;
    std::vector<std::uint8_t> modValues;
    std::vector<std::uint8_t> biomoValues;
    std::vector<Vector3> vertexData;
    std::vector<int> indexMap;
};

}
