#include "planet_raw_data.hh"

#include <array>
#include <cmath>
#include <limits>

namespace dspugen::terrain {

namespace {

constexpr float Pi = 3.14159265358979323846f;

constexpr std::array<Vector3, 6> poles = {{
    {1.0f, 0.0f, 0.0f},
    {-1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, -1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, -1.0f},
}};

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
      indexMap(static_cast<std::size_t>(indexMapCornerStride * 8)) {
    initModData();
}

void PlanetRawData::initModData() {
    modValues.assign(static_cast<std::size_t>(dataLength() / 2), 0);
}

void PlanetRawData::calcVerts() {
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
    auto radius = Pi / static_cast<float>(precisionValue * 2) * 0.25f;
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

float PlanetRawData::queryHeight(Vector3 value) const {
    value.normalize();
    auto hash = positionHash(value);
    auto start = indexMap[static_cast<std::size_t>(hash)];
    auto radius = Pi / static_cast<float>(precisionValue * 2) * 1.2f;
    auto radiusSquared = radius * radius;
    float weightSum = 0.0f;
    float heightSum = 0.0f;
    auto rowStride = stride();
    for (int i = -1; i <= 3; ++i) {
        for (int j = -1; j <= 3; ++j) {
            auto index = start + i + j * rowStride;
            if (index < 0 || index >= dataLength()) continue;
            auto distance = (vertexData[static_cast<std::size_t>(index)] - value).sqrMagnitude();
            if (distance > radiusSquared) continue;
            auto weight = 1.0f - std::sqrt(distance) / radius;
            auto height = static_cast<float>(heightValues[static_cast<std::size_t>(index)]);
            weightSum += weight;
            heightSum += height * weight;
        }
    }
    if (weightSum == 0.0f) return static_cast<float>(heightValues[0]) * 0.01f;
    return heightSum / weightSum * 0.01f;
}

}
