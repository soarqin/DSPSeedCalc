#pragma once

#include "planet.hh"
#include "star.hh"

#include <raylib.h>
#include <raymath.h>

#include <cstddef>
#include <string>
#include <vector>

namespace viewer {

struct StarMassMaterialData {
    Vector4 color0;
    Vector4 color1;
    Vector4 color2;
    Vector4 color3;
    float multiplier;
    float noiseTile;
    float noiseSpeed;
    float chaosTile;
    float chaosDistort;
    float chaosOverlay;
    float spotIntensity;
    float rimPower;
    float rotSpeed;
};

struct StarSpotMaterialData {
    Vector4 centerColor;
    Vector4 glowColor;
    float multiplier;
    float glowPower;
    float centerMultiplier;
    float centerSize;
    float centerBlur;
    float centerBlurPower;
};

struct PlanetThumbMaterialData {
    Vector4 color;
    Vector4 rimColor;
};

struct Line3DCache {
    Vector3 start{};
    Vector3 end{};
};

struct PanelRow {
    const char *key = nullptr;
    std::string value;
};

struct PanelData {
    std::string title;
    std::string name;
    std::string subtitle;
    std::vector<PanelRow> rows;
    std::vector<PanelRow> detailRows;
    std::string footerKey;
    std::string footerValue;
};

struct GalaxyLineData {
    std::size_t start = 0;
    std::size_t end = 0;
};

struct StarViewData {
    const dspugen::Star *data = nullptr;
    Vector3 position{};
    Vector4 galaxyColor{};
    float galaxyScale = 1.2f;
    float radius = 1.0f;
    Vector3 selectionSize{};
    float labelWidth = 0.0f;
    PanelData panel;
    StarMassMaterialData massMaterial{};
    StarSpotMaterialData spotMaterial{};
};

struct PlanetViewData {
    const dspugen::Planet *data = nullptr;
    Vector3 position{};
    float radius = 0.25f;
    Color color{};
    float flickerPhase = 0.0f;
    float selectionRadius = 0.0f;
    int parentIndex = -1;
    float orbitRadius = 0.0f;
    float orbitInclination = 0.0f;
    float orbitLongitude = 0.0f;
    float orbitPhase = 0.0f;
    double orbitalPeriod = 3600.0;
    Matrix orbitRotation{};
    std::vector<Vector3> orbitPoints;
    float labelWidth = 0.0f;
    std::string name;
    PanelData panel;
    PlanetThumbMaterialData thumbMaterial{};
};

struct SystemViewCache {
    std::vector<PlanetViewData> planets;
    StarViewData star;
    std::string summary;
    float radius = 8.0f;
};

struct OrbitParameters {
    float inclination = 0.0f;
    float longitude = 0.0f;
    float phase = 0.0f;
    double period = 3600.0;
};

std::vector<GalaxyLineData> CreateGalaxyLines(const dspugen::Galaxy *galaxy,
                                              const std::vector<StarViewData> &stars);

enum class ViewMode {
    Galaxy,
    System,
};

}
