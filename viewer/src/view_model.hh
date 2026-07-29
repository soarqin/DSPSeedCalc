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

// Every constant `VF Shaders/Forward/Planet Thumb New` binds. The six colours are `Color`
// properties, so `PlanetThumbMaterial(...)` returns them already converted to linear;
// `heightSettings` is a `Vector` property and stays as serialised.
struct PlanetThumbMaterialData {
    Vector4 color;
    Vector4 shoreLineColor;
    Vector4 rimColor;
    Vector4 colorBio0;
    Vector4 colorBio1;
    Vector4 colorBio2;
    Vector4 heightSettings;
    float shoreInvThick;
    float shoreHeight;
    float wireIntensity;
    float bioStrength;
    float farHeight;
    float diameter;
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
    int parentIndex = -1;
    float orbitRadius = 0.0f;
    float orbitInclination = 0.0f;
    float orbitLongitude = 0.0f;
    float orbitPhase = 0.0f;
    // `orbitAngle` is the phase the planet currently sits at, so the orbit shader can put the
    // bright end of its colour ramp on the planet the way `Quaternion.Euler(0, -phase, 0)` does.
    float orbitAngle = 0.0f;
    // `UIStarmapPlanet._OnInit`: full width `0.7 * star.orbitScaler` around a star, `0.1` around a
    // planet. The ribbon is only the canvas the analytic orbit shader rasterises into.
    float orbitWidth = 0.7f;
    // `orbitMat.SetInt("_Stencil", planet.index + 101)`.
    int orbitStencilRef = 101;
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
    // `UIStarmap` keeps every other star of the cluster in the scene while a system is in view,
    // placed at `(uPosition - viewTargetUPos) * 0.00025`. Their distance puts them past the point
    // where `UIStarmapStarObject` stops drawing the mass sphere, so only the spot quad survives.
    std::vector<StarViewData> otherStars;
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
