/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "galaxy.hh"
#include "protoset.hh"
#include "settings.hh"
#include "util/dotnet35random.hh"

#include <raylib.h>
#include <rcamera.h>
#include <raymath.h>
#include <rlgl.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#else
#include <external/glad.h>
#endif

#ifndef DSP_VIEWER_DEFAULT_FONT
#define DSP_VIEWER_DEFAULT_FONT ""
#endif

namespace {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;
constexpr float kPanelWidth = 326.0f;
constexpr float kCameraMoveSpeed = 0.09f;
constexpr float kCameraRotationSpeed = 0.03f;
constexpr float kCameraPanSpeed = 0.2f;
constexpr float kCameraMouseSensitivity = 0.003f;
constexpr float kGalaxyCameraDistance = 80.9000015f;
constexpr float kGalaxyCameraTargetHeight = 11.0f;
constexpr float kGalaxyCameraPitchBegin = -22.0f;
constexpr float kGalaxyCameraPitchMin = -81.1999969f;
constexpr float kGalaxyCameraPitchMax = -4.9000001f;
constexpr float kGalaxyGridHeight = -15.0f;
constexpr float kSystemOrbitScale = 10.0f;
constexpr int kOrbitSegments = 72;
constexpr int kEnterButtonTextWidth = 243;
constexpr int kBackButtonTextWidth = 80;
constexpr int kViewerFontSize = 64;
constexpr int kMaxBloomIterations = 16;

Font viewerFont{};
bool hasCustomFont = false;
Shader starSurfaceShader{};
Shader starSpotShader{};
Shader starBlackShader{};
Shader galaxyStarPointShader{};
Shader galaxyLineShader{};
Shader bloomPrefilterShader{};
Shader bloomDownsampleFirstShader{};
Shader bloomDownsampleShader{};
Shader bloomUpsampleShader{};
Shader bloomCompositeShader{};
Shader galaxyFxaaShader{};
Model starMassModel{};
Model starBlackModel{};
Model starSpotModel{};
Model galaxyStarPointModel{};
Texture2D galaxyStarPointTexture{};
Texture2D galaxyLineTexture{};
Texture2D bloomLensDirtTexture{};
Texture2D galaxyColorGradingLut{};
bool hasStarSurfaceShader = false;
bool hasStarSpotShader = false;
bool hasStarBlackShader = false;
bool hasGalaxyStarPointShader = false;
bool hasGalaxyLineShader = false;
bool hasBloomShaders = false;
bool hasGalaxyFxaaShader = false;
bool hasStarModels = false;
bool hasGalaxyStarPointModel = false;
RenderTexture2D galaxySceneTarget{};
RenderTexture2D galaxySceneMultisampleTarget{};
RenderTexture2D galaxyFxaaTarget{};
RenderTexture2D bloomPrefilterTarget{};
std::array<RenderTexture2D, kMaxBloomIterations> bloomDownLevels{};
std::array<RenderTexture2D, kMaxBloomIterations> bloomUpLevels{};
int bloomTargetWidth = 0;
int bloomTargetHeight = 0;
int bloomIterationCount = 0;
float bloomSampleScale = 1.0f;

Texture2D LoadGalaxyLineTexture(const char *path) {
    int size = 0;
    unsigned char *data = LoadFileData(path, &size);
    constexpr int expectedSize = 699056;
    if (data == nullptr || size != expectedSize) {
        if (data != nullptr) UnloadFileData(data);
        return {};
    }
    Texture2D texture;
    texture.id = rlLoadTexture(data, 256, 128, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 9);
    texture.width = 256;
    texture.height = 128;
    texture.mipmaps = 9;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
    UnloadFileData(data);
    return texture;
}

std::string ViewerAssetPath(const char *name) {
#if defined(__EMSCRIPTEN__)
    return name;
#else
    return std::string(GetApplicationDirectory()) + name;
#endif
}

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
    float labelWidth = 0.0f;
    std::string name;
    PanelData panel;
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

enum class ViewMode {
    Galaxy,
    System,
};

#if defined(__EMSCRIPTEN__)
struct WebMouseState {
    bool middleDown = false;
    bool rightDown = false;
    Vector2 delta{};
};

WebMouseState webMouse;

EM_BOOL HandleWebMouse(int eventType, const EmscriptenMouseEvent *event, void *) {
    webMouse.middleDown = (event->buttons & 4) != 0;
    webMouse.rightDown = (event->buttons & 2) != 0;
    if (eventType == EMSCRIPTEN_EVENT_MOUSEMOVE && (webMouse.middleDown || webMouse.rightDown)) {
        webMouse.delta.x += static_cast<float>(event->movementX);
        webMouse.delta.y += static_cast<float>(event->movementY);
    }
    return (event->button == 1 || event->button == 2 || webMouse.middleDown || webMouse.rightDown) ? EM_TRUE : EM_FALSE;
}

EM_BOOL HandleWebMouseLeave(int, const EmscriptenMouseEvent *, void *) {
    webMouse.middleDown = false;
    webMouse.rightDown = false;
    webMouse.delta = Vector2 {};
    return EM_FALSE;
}

EM_BOOL HandleWebFocus(int eventType, const EmscriptenFocusEvent *, void *) {
    if (eventType == EMSCRIPTEN_EVENT_BLUR) {
        webMouse.middleDown = false;
        webMouse.rightDown = false;
        webMouse.delta = Vector2 {};
    }
    return EM_FALSE;
}

void InitWebMouse() {
    emscripten_set_mousedown_callback("#canvas", nullptr, true, HandleWebMouse);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, HandleWebMouse);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, HandleWebMouse);
    emscripten_set_mouseleave_callback("#canvas", nullptr, true, HandleWebMouseLeave);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, HandleWebFocus);
}
#endif

Color WithAlpha(Color color, float alpha) {
    color.a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    return color;
}

Color AddColor(Color color, int amount) {
    color.r = static_cast<unsigned char>(std::clamp(static_cast<int>(color.r) + amount, 0, 255));
    color.g = static_cast<unsigned char>(std::clamp(static_cast<int>(color.g) + amount, 0, 255));
    color.b = static_cast<unsigned char>(std::clamp(static_cast<int>(color.b) + amount, 0, 255));
    return color;
}

Color LinearColor(Color color) {
    auto convert = [](unsigned char value) {
        float srgb = static_cast<float>(value) / 255.0f;
        float linear = srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
        return static_cast<unsigned char>(std::round(linear * 255.0f));
    };
    return Color {convert(color.r), convert(color.g), convert(color.b), color.a};
}

Color LinearBackground(Color color) {
    return LinearColor(color);
}

float GammaToLinearSpace(float value) {
    if (value <= 0.04045f) return value / 12.92f;
    return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

Vector4 GalaxyStarColor(const dspugen::Star *star) {
    static constexpr struct {
        float position;
        Vector4 color;
    } keys[] = {
        {0.0f, {1.0f, 0.229039833f, 0.174528301f, 1.0f}},
        {0.06999313f, {1.0f, 0.437254906f, 0.176470578f, 1.0f}},
        {0.31999695f, {1.0f, 0.951527596f, 0.155660391f, 1.0f}},
        {0.50000763f, {0.987870634f, 1.0f, 0.617924571f, 1.0f}},
        {0.723537f, {0.976415098f, 1.0f, 0.97796154f, 1.0f}},
        {0.8200046f, {0.580000043f, 0.896296144f, 1.0f, 1.0f}},
        {0.919996977f, {0.117647052f, 0.654785156f, 1.0f, 1.0f}},
        {1.0f, {0.0990566015f, 0.340047508f, 1.0f, 1.0f}},
    };
    if (star->type == dspugen::EStarType::NeutronStar)
        return Vector4 {0.502143919f, 0.382075489f, 1.0f, 1.0f};
    if (star->type == dspugen::EStarType::WhiteDwarf)
        return Vector4 {0.46226418f, 0.46226418f, 0.46226418f, 1.0f};
    if (star->type == dspugen::EStarType::BlackHole) return Vector4 {0.0f, 0.0f, 0.0f, 1.0f};
    float value = std::clamp(star->color, 0.0f, 1.0f);
    for (std::size_t i = 1; i < std::size(keys); ++i) {
        if (value > keys[i].position) continue;
        float amount = (value - keys[i - 1].position) / (keys[i].position - keys[i - 1].position);
        return Vector4Lerp(keys[i - 1].color, keys[i].color, amount);
    }
    return keys[std::size(keys) - 1].color;
}

Vector4 GalaxyLineColor(Vector4 color) {
    Vector4 result = Vector4Lerp(color, Vector4 {1.0f, 1.0f, 1.0f, 1.0f}, 0.65f);
    result.x = GammaToLinearSpace(result.x);
    result.y = GammaToLinearSpace(result.y);
    result.z = GammaToLinearSpace(result.z);
    return result;
}

float GalaxyStarScale(const dspugen::Star *star) {
    switch (star->type) {
        case dspugen::EStarType::GiantStar: return 3.0f;
        case dspugen::EStarType::WhiteDwarf:
        case dspugen::EStarType::NeutronStar: return 0.6f;
        case dspugen::EStarType::BlackHole: return 0.8f;
        default: return 1.2f;
    }
}

void SortedAdd(std::vector<std::size_t> &nodes, std::size_t node) {
    auto position = std::lower_bound(nodes.begin(), nodes.end(), node);
    if (position == nodes.end() || *position != node) nodes.insert(position, node);
}

void RemoveNode(std::vector<std::size_t> &nodes, std::size_t node) {
    auto position = std::lower_bound(nodes.begin(), nodes.end(), node);
    if (position != nodes.end() && *position == node) nodes.erase(position);
}

std::vector<GalaxyLineData> CreateGalaxyLines(const dspugen::Galaxy *galaxy,
                                              const std::vector<StarViewData> &stars) {
    struct GraphNode {
        std::vector<std::size_t> connections;
        std::vector<std::size_t> lines;
    };
    std::vector<GraphNode> nodes(stars.size());
    std::vector<int> state(stars.size());
    for (std::size_t nodeIndex = 0; nodeIndex < stars.size(); ++nodeIndex) {
        const auto &position = galaxy->stars[nodeIndex]->position;
        for (std::size_t otherIndex = 0; otherIndex < nodeIndex; ++otherIndex) {
            const auto &otherPosition = galaxy->stars[otherIndex]->position;
            double dx = position.x - otherPosition.x;
            double dy = position.y - otherPosition.y;
            double dz = position.z - otherPosition.z;
            if (dx * dx + dy * dy + dz * dz >= 64.0) continue;
            SortedAdd(nodes[nodeIndex].connections, otherIndex);
            SortedAdd(nodes[otherIndex].connections, nodeIndex);
        }

        std::fill(state.begin(), state.end(), 0);
        Vector3 nodePosition = stars[nodeIndex].position;
        const auto &connections = nodes[nodeIndex].connections;
        for (std::size_t i = 0; i < connections.size(); ++i) {
            std::size_t firstIndex = connections[i];
            Vector3 firstPosition = stars[firstIndex].position;
            for (std::size_t j = i + 1; j < connections.size(); ++j) {
                std::size_t secondIndex = connections[j];
                if (!std::binary_search(nodes[firstIndex].connections.begin(),
                                        nodes[firstIndex].connections.end(), secondIndex)) continue;
                Vector3 secondPosition = stars[secondIndex].position;
                float firstDistance = Vector3DistanceSqr(firstPosition, nodePosition);
                float secondDistance = Vector3DistanceSqr(secondPosition, nodePosition);
                float otherDistance = Vector3DistanceSqr(firstPosition, secondPosition);
                float largest = std::max(firstDistance, std::max(secondDistance, otherDistance));
                float smallest = std::min(firstDistance, std::min(secondDistance, otherDistance));
                float middle = (firstDistance + secondDistance + otherDistance - largest - smallest) * 1.001f;
                smallest *= 1.01f;
                if (firstDistance <= middle || firstDistance <= smallest) {
                    if (state[i] == 0) {
                        SortedAdd(nodes[nodeIndex].lines, firstIndex);
                        SortedAdd(nodes[firstIndex].lines, nodeIndex);
                        state[i] = 1;
                    }
                } else {
                    state[i] = -1;
                    RemoveNode(nodes[nodeIndex].lines, firstIndex);
                    RemoveNode(nodes[firstIndex].lines, nodeIndex);
                }
                if (secondDistance <= middle || secondDistance <= smallest) {
                    if (state[j] == 0) {
                        SortedAdd(nodes[nodeIndex].lines, secondIndex);
                        SortedAdd(nodes[secondIndex].lines, nodeIndex);
                        state[j] = 1;
                    }
                } else {
                    state[j] = -1;
                    RemoveNode(nodes[nodeIndex].lines, secondIndex);
                    RemoveNode(nodes[secondIndex].lines, nodeIndex);
                }
                if (otherDistance > middle && otherDistance > smallest) {
                    RemoveNode(nodes[firstIndex].lines, secondIndex);
                    RemoveNode(nodes[secondIndex].lines, firstIndex);
                }
            }
            if (state[i] == 0) {
                SortedAdd(nodes[nodeIndex].lines, firstIndex);
                SortedAdd(nodes[firstIndex].lines, nodeIndex);
                state[i] = 1;
            }
        }
    }

    std::vector<GalaxyLineData> result;
    for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
        for (std::size_t otherIndex: nodes[nodeIndex].lines) {
            if (otherIndex > nodeIndex) result.push_back(GalaxyLineData {nodeIndex, otherIndex});
        }
    }
    return result;
}

Vector4 LerpColor(Vector4 left, Vector4 right, float t) {
    return Vector4Lerp(left, right, t);
}

constexpr float kSpectralParams[] = {-3.5999999046325684f, -3.0f, -2.0f, -1.0f, -0.30000001192092896f,
                                     0.0f, 0.25f, 1.0f, 1.600000023841858f};

struct SpectralBlend {
    std::size_t material = 0;
    std::size_t target = 0;
    float amount = 0.0f;
    bool hasTarget = false;
};

SpectralBlend FindSpectralBlend(float classFactor) {
    SpectralBlend result;
    for (std::size_t i = 0; i + 1 < std::size(kSpectralParams); ++i) {
        if (classFactor >= kSpectralParams[i] && classFactor <= kSpectralParams[i + 1]) {
            result.material = i;
            result.target = i + 1;
            result.amount = std::clamp((classFactor - kSpectralParams[i]) /
                                       (kSpectralParams[i + 1] - kSpectralParams[i]), 0.0f, 1.0f);
            result.hasTarget = true;
        }
    }
    if (classFactor <= kSpectralParams[0]) result.material = 0;
    if (classFactor >= kSpectralParams[std::size(kSpectralParams) - 1])
        result.material = std::size(kSpectralParams) - 1;
    return result;
}

float LerpFloat(float left, float right, float amount) {
    return left + (right - left) * amount;
}

StarMassMaterialData StarMassMaterial(const dspugen::Star *star) {
    static const StarMassMaterialData mainMaterials[] = {
        {{1.0f, 0.629063308f, 0.36470592f, 1.0f}, {0.528301895f, 0.255579025f, 0.0f, 1.0f}, {0.264150918f, 0.0138839362f, 0.00373797794f, 1.0f}, {1.0f, 0.282301843f, 0.0784313679f, 1.0f}, 4.0f, 200.0f, 0.300000012f, 1.29999995f, 0.180000007f, 0.75f, 12.0f, 3.0f, 0.0399999991f},
        {{1.0f, 0.813083529f, 0.363207519f, 1.0f}, {0.632075429f, 0.372084856f, 0.0f, 1.0f}, {0.301886797f, 0.0137221254f, 0.0f, 1.0f}, {1.0f, 0.432450503f, 0.0801886916f, 1.0f}, 4.0f, 200.0f, 0.300000012f, 1.29999995f, 0.180000007f, 0.75f, 12.0f, 3.0f, 0.0399999991f},
        {{0.990566015f, 0.933518291f, 0.406505883f, 1.0f}, {0.575471699f, 0.473466545f, 0.0407173298f, 1.0f}, {0.41568628f, 0.226671472f, 0.0f, 1.0f}, {1.0f, 0.56016314f, 0.26886791f, 1.0f}, 4.0f, 200.0f, 0.300000012f, 1.29999995f, 0.180000007f, 0.800000012f, 12.0f, 4.0f, 0.0399999991f},
        {{0.801886797f, 0.783705056f, 0.624109983f, 1.0f}, {0.556603789f, 0.502054989f, 0.312433243f, 1.0f}, {0.386792481f, 0.253650099f, 0.133187965f, 1.0f}, {0.726999998f, 0.643031478f, 0.391125977f, 1.0f}, 4.0f, 250.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.699999988f, 12.0f, 3.5f, 0.0399999991f},
        {{0.894999981f, 0.878270984f, 0.815537393f, 1.0f}, {0.490196079f, 0.46526733f, 0.36470589f, 1.0f}, {0.427450985f, 0.422490686f, 0.407843143f, 1.0f}, {0.796999991f, 0.796999991f, 0.665292382f, 1.0f}, 4.0f, 250.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.899999976f, 12.0f, 5.0f, 0.0399999991f},
        {{0.897672892f, 0.950999975f, 0.866565406f, 1.0f}, {0.506150007f, 0.573000014f, 0.487050027f, 1.0f}, {0.405999988f, 0.405999988f, 0.405999988f, 1.0f}, {0.911330044f, 1.0f, 0.985221684f, 1.0f}, 4.0f, 250.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.800000012f, 12.0f, 7.0f, 0.0399999991f},
        {{0.78047061f, 0.828764975f, 0.839215696f, 1.0f}, {0.656894147f, 0.737478256f, 0.776470602f, 1.0f}, {0.36470589f, 0.386221647f, 0.392156869f, 1.0f}, {0.74125427f, 0.854135633f, 0.888000011f, 1.0f}, 4.0f, 250.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.899999976f, 12.0f, 5.0f, 0.0399999991f},
        {{0.630784988f, 0.92203629f, 0.990566015f, 1.0f}, {0.313725501f, 0.39416796f, 0.556862772f, 1.0f}, {0.0813011751f, 0.205825761f, 0.273584902f, 1.0f}, {0.384700954f, 0.570500672f, 0.858490586f, 1.0f}, 4.0f, 280.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.699999988f, 12.0f, 4.0f, 0.0399999991f},
        {{0.458823562f, 0.737789989f, 1.0f, 1.0f}, {0.164705873f, 0.39788875f, 0.784313738f, 1.0f}, {0.0117647108f, 0.161571428f, 0.301960796f, 1.0f}, {0.286274582f, 0.401634216f, 0.992156863f, 1.0f}, 5.0f, 300.0f, 0.300000012f, 1.0f, 0.180000007f, 0.699999988f, 12.0f, 3.5f, 0.0399999991f},
    };
    static const StarMassMaterialData specialMaterials[] = {
        {{0.816037714f, 0.850495577f, 1.0f, 1.0f}, {0.411764711f, 0.499932766f, 0.549019635f, 1.0f}, {0.129505023f, 0.121568628f, 0.235294119f, 1.0f}, {0.57558459f, 0.372549057f, 1.0f, 1.0f}, 6.0f, 70.0f, 8.0f, 0.899999976f, 0.25f, 0.949999988f, 5.0f, 2.0f, 0.300000012f},
        {{1.0f, 1.0f, 1.0f, 1.0f}, {0.740101993f, 0.752798617f, 0.772549033f, 1.0f}, {0.122821964f, 0.161190778f, 0.216999978f, 1.0f}, {0.552910268f, 0.595265687f, 0.669811308f, 1.0f}, 4.0f, 60.0f, 0.0500000007f, 1.5f, 0.300000012f, 0.850000024f, 5.0f, 2.79999995f, 1.5f},
        {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.264150918f, 0.264150918f, 0.264150918f, 1.0f}, 6.0f, 6.0f, 0.400000006f, 0.899999976f, 0.25f, 0.850000024f, 0.0f, 4.0f, 0.300000012f},
    };

    if (star->type == dspugen::EStarType::NeutronStar) return specialMaterials[0];
    if (star->type == dspugen::EStarType::WhiteDwarf) return specialMaterials[1];
    if (star->type == dspugen::EStarType::BlackHole) return specialMaterials[2];
    auto blend = FindSpectralBlend(star->classFactor);
    StarMassMaterialData result = mainMaterials[blend.material];
    if (blend.hasTarget) {
        const auto &target = mainMaterials[blend.target];
        result.color0 = LerpColor(result.color0, target.color0, blend.amount);
        result.color1 = LerpColor(result.color1, target.color1, blend.amount);
        result.color2 = LerpColor(result.color2, target.color2, blend.amount);
        result.color3 = LerpColor(result.color3, target.color3, blend.amount);
        result.multiplier = LerpFloat(result.multiplier, target.multiplier, blend.amount);
        result.noiseTile = LerpFloat(result.noiseTile, target.noiseTile, blend.amount);
        result.noiseSpeed = LerpFloat(result.noiseSpeed, target.noiseSpeed, blend.amount);
        result.chaosTile = LerpFloat(result.chaosTile, target.chaosTile, blend.amount);
        result.chaosDistort = LerpFloat(result.chaosDistort, target.chaosDistort, blend.amount);
        result.chaosOverlay = LerpFloat(result.chaosOverlay, target.chaosOverlay, blend.amount);
        result.spotIntensity = LerpFloat(result.spotIntensity, target.spotIntensity, blend.amount);
        result.rimPower = LerpFloat(result.rimPower, target.rimPower, blend.amount);
        result.rotSpeed = LerpFloat(result.rotSpeed, target.rotSpeed, blend.amount);
    }
    if (star->type == dspugen::EStarType::GiantStar) {
        result.noiseTile *= 3.0f;
        result.chaosTile /= 3.0f;
    }
    return result;
}

StarSpotMaterialData StarSpotMaterial(const dspugen::Star *star) {
    static const StarSpotMaterialData mainMaterials[] = {
        {{1.0f, 0.466861874f, 0.258823514f, 1.0f}, {1.0f, 0.0f, 0.15857029f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{1.0f, 0.571663201f, 0.334905684f, 1.0f}, {1.0f, 0.426606327f, 0.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{1.0f, 0.789915204f, 0.334905684f, 1.0f}, {1.0f, 0.591718853f, 0.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{1.0f, 0.726871908f, 0.353773594f, 1.0f}, {1.0f, 0.876956701f, 0.627358496f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.930000007f, 0.99667722f, 1.0f, 1.0f}, {0.908999979f, 0.98862499f, 1.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.957547188f, 1.0f, 0.984421909f, 1.0f}, {0.99000001f, 1.0f, 0.993749976f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{1.0f, 0.99902153f, 0.908999979f, 1.0f}, {1.0f, 0.995625019f, 0.930000007f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.391509414f, 0.82864356f, 1.0f, 1.0f}, {0.419811308f, 0.640218198f, 1.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.221698105f, 0.583651304f, 1.0f, 1.0f}, {0.221698105f, 0.391599625f, 1.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
    };
    static const StarSpotMaterialData specialMaterials[] = {
        {{0.433218956f, 0.380392134f, 1.0f, 1.0f}, {0.617746413f, 0.258823484f, 1.0f, 1.0f}, 0.600000024f, 5.0f, 12.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.929245293f, 0.941550434f, 1.0f, 1.0f}, {0.909803927f, 0.911484003f, 1.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.837492347f, 0.580188632f, 1.0f, 1.0f}, {1.0f, 0.333333343f, 0.760824859f, 1.0f}, 0.600000024f, 5.0f, 5.0f, 0.319999993f, 0.0500000007f, 10.0f},
    };

    if (star->type == dspugen::EStarType::NeutronStar) return specialMaterials[0];
    if (star->type == dspugen::EStarType::WhiteDwarf) return specialMaterials[1];
    if (star->type == dspugen::EStarType::BlackHole) return specialMaterials[2];
    auto blend = FindSpectralBlend(star->classFactor);
    StarSpotMaterialData result = mainMaterials[blend.material];
    if (!blend.hasTarget) return result;
    const auto &target = mainMaterials[blend.target];
    result.centerColor = LerpColor(result.centerColor, target.centerColor, blend.amount);
    result.glowColor = LerpColor(result.glowColor, target.glowColor, blend.amount);
    result.multiplier = LerpFloat(result.multiplier, target.multiplier, blend.amount);
    result.glowPower = LerpFloat(result.glowPower, target.glowPower, blend.amount);
    result.centerMultiplier = LerpFloat(result.centerMultiplier, target.centerMultiplier, blend.amount);
    result.centerBlurPower = LerpFloat(result.centerBlurPower, target.centerBlurPower, blend.amount);
    return result;
}

const char *SpectrName(dspugen::ESpectrType spectr) {
    switch (spectr) {
        case dspugen::ESpectrType::M: return "M";
        case dspugen::ESpectrType::K: return "K";
        case dspugen::ESpectrType::G: return "G";
        case dspugen::ESpectrType::F: return "F";
        case dspugen::ESpectrType::A: return "A";
        case dspugen::ESpectrType::B: return "B";
        case dspugen::ESpectrType::O: return "O";
        default: return "X";
    }
}

const char *PlanetTypeName(dspugen::EPlanetType type) {
    switch (type) {
        case dspugen::EPlanetType::Vocano: return "Volcanic";
        case dspugen::EPlanetType::Ocean: return "Ocean";
        case dspugen::EPlanetType::Desert: return "Desert";
        case dspugen::EPlanetType::Ice: return "Ice";
        case dspugen::EPlanetType::Gas: return "Gas giant";
        default: return "Unknown";
    }
}

Color PlanetColor(dspugen::EPlanetType type) {
    switch (type) {
        case dspugen::EPlanetType::Vocano: return Color {224, 78, 46, 255};
        case dspugen::EPlanetType::Ocean: return Color {45, 152, 224, 255};
        case dspugen::EPlanetType::Desert: return Color {214, 155, 75, 255};
        case dspugen::EPlanetType::Ice: return Color {156, 214, 232, 255};
        case dspugen::EPlanetType::Gas: return Color {218, 132, 202, 255};
        default: return Color {160, 160, 175, 255};
    }
}

std::string ToRoman(int value) {
    static constexpr struct {
        int value;
        const char *text;
    } numerals[] = {
        {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"},
        {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"},
    };
    std::string result;
    for (const auto &numeral: numerals) {
        while (value >= numeral.value) {
            result += numeral.text;
            value -= numeral.value;
        }
    }
    return result;
}

std::string PlanetName(const dspugen::Planet *planet) {
    int displayNumber = planet->index + 1;
    return planet->star->name + " " +
           (planet->star->planets.size() > 20 ? std::to_string(displayNumber) : ToRoman(displayNumber));
}

std::string SingularityName(int singularity) {
    std::string result;
    auto append = [&result](const char *name) {
        if (!result.empty()) result += ", ";
        result += name;
    };
    if ((singularity & dspugen::EPlanetSingularity::TidalLocked) != 0) append("tidal locked");
    if ((singularity & dspugen::EPlanetSingularity::TidalLocked2) != 0) append("2:1 resonance");
    if ((singularity & dspugen::EPlanetSingularity::TidalLocked4) != 0) append("4:1 resonance");
    if ((singularity & dspugen::EPlanetSingularity::LaySide) != 0) append("lay side");
    if ((singularity & dspugen::EPlanetSingularity::ClockwiseRotate) != 0) append("clockwise");
    if ((singularity & dspugen::EPlanetSingularity::MultipleSatellites) != 0) append("multiple satellites");
    return result.empty() ? "none" : result;
}

const char *ThemeName(int theme) {
    static const char *names[] = {
        "Unknown", "Mediterranean", "Gas Giant", "Gas Giant", "Ice Giant", "Ice Giant",
        "Arid Desert", "Ashen Gelisol", "Oceanic Jungle", "Lava", "Ice Field Gelisol",
        "Barren Desert", "Gobi", "Volcanic Ash", "Red Stone", "Prairie", "Waterworld",
        "Rocky Salt Lake", "Sakura Ocean", "Hurricane Stone Forest", "Scarlet Ice Lake",
        "Gas Giant", "Savanna", "Crystal Desert", "Frozen Tundra", "Pandora Swamp",
    };
    return theme > 0 && theme < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[theme] : names[0];
}

const char *VeinName(int veinType) {
    static const char *names[] = {
        "", "Iron veins", "Copper veins", "Silicon veins", "Titanium veins", "Stone veins",
        "Coal veins", "Crude oil seep", "Fire ice vein", "Kimberlite vein", "Fractal silicon vein",
        "Organic crystal vein", "Optical grating crystal vein", "Spiniform stalagmite crystal vein",
        "Unipolar magnet vein",
    };
    return veinType > 0 && veinType < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[veinType] : "Unknown";
}

float Hash01(int value) {
    unsigned int hash = static_cast<unsigned int>(value);
    hash ^= hash >> 16;
    hash *= 0x7feb352dU;
    hash ^= hash >> 15;
    hash *= 0x846ca68bU;
    hash ^= hash >> 16;
    return static_cast<float>(hash & 0x00ffffffU) / 16777216.0f;
}

template<typename... Args>
std::string FormatString(const char *format, Args... args) {
    int length = std::snprintf(nullptr, 0, format, args...);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    std::snprintf(result.data(), result.size() + 1, format, args...);
    return result;
}

PanelData CreateStarPanel(const dspugen::Star *star, int galaxySeed) {
    PanelData panel;
    panel.title = "STAR PARAMETERS";
    panel.name = star->name;
    panel.subtitle = star->typeName();
    panel.rows = {
        {"galaxy seed", std::to_string(galaxySeed)},
        {"star ID", std::to_string(star->id)},
        {"generation seed", std::to_string(star->seed)},
        {"spectrum", SpectrName(star->spectr)},
        {"position X", FormatString("%.3f", star->position.x)},
        {"position Y", FormatString("%.3f", star->position.y)},
        {"position Z", FormatString("%.3f", star->position.z)},
        {"mass", FormatString("%.4f solar", star->mass)},
        {"temperature", FormatString("%.0f K", star->temperature)},
        {"luminosity", FormatString("%.4f raw", star->luminosity)},
        {"radius", FormatString("%.4f solar", star->radius)},
        {"habitable radius", FormatString("%.4f AU", star->habitableRadius)},
        {"orbit scaler", FormatString("%.4f", star->orbitScaler)},
        {"dyson radius", FormatString("%.4f AU", star->dysonRadius)},
        {"planets", std::to_string(star->planets.size())},
    };
    panel.footerValue = "Original-style sprite lighting";
    return panel;
}

PanelData CreatePlanetPanel(const dspugen::Planet *planet, const std::string &name) {
    PanelData panel;
    panel.title = "PLANET PARAMETERS";
    panel.name = name;
    panel.subtitle = PlanetTypeName(planet->type);
    panel.rows = {
        {"star", planet->star->name},
        {"orbit parent", planet->orbitAround == 0 ? "star" : std::to_string(planet->orbitAround)},
        {"orbit radius", FormatString("%.4f AU", planet->orbitRadius)},
        {"sun distance", FormatString("%.4f AU", planet->sunDistance)},
        {"real radius", FormatString("%.1f km", planet->realRadius())},
        {"luminosity", FormatString("%.2f", planet->luminosity)},
        {"theme", ThemeName(planet->theme)},
    };
    for (int i = 1; i < 15; ++i) {
        if (planet->veinSpot[i] > 0) panel.detailRows.push_back({VeinName(i), std::to_string(planet->veinSpot[i])});
    }
    if (panel.detailRows.empty()) panel.detailRows.push_back({"None", ""});
    panel.footerKey = "singularities";
    panel.footerValue = SingularityName(planet->singularity);
    return panel;
}

void ApplyGalaxyCamera(Camera3D &camera, float yaw, float pitch) {
    float yawRadians = yaw * DEG2RAD;
    float pitchRadians = pitch * DEG2RAD;
    float sinYaw = std::sin(yawRadians);
    float cosYaw = std::cos(yawRadians);
    float sinPitch = std::sin(pitchRadians);
    float cosPitch = std::cos(pitchRadians);
    Vector3 forward {sinYaw * cosPitch, sinPitch, -cosYaw * cosPitch};
    camera.target = Vector3 {0.0f, kGalaxyCameraTargetHeight, 0.0f};
    camera.position = camera.target - forward * kGalaxyCameraDistance;
    camera.up = Vector3 {-sinYaw * sinPitch, cosPitch, cosYaw * sinPitch};
}

void ResetCamera(Camera3D &camera, ViewMode mode, float &galaxyYaw, float &galaxyPitch) {
    camera.up = Vector3 {0.0f, 1.0f, 0.0f};
    camera.fovy = mode == ViewMode::Galaxy ? 60.0f : 52.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    if (mode == ViewMode::Galaxy) {
        galaxyYaw = 0.0f;
        galaxyPitch = kGalaxyCameraPitchBegin;
        ApplyGalaxyCamera(camera, galaxyYaw, galaxyPitch);
    } else {
        camera.position = Vector3 {0.0f, 24.0f, 30.0f};
        camera.target = Vector3 {0.0f, 0.0f, 0.0f};
    }
}

void CameraControl(Camera *camera, ViewMode mode, float &galaxyYaw, float &galaxyPitch) {
    if (mode == ViewMode::Galaxy) {
        if (IsKeyDown(KEY_DOWN)) galaxyPitch -= kCameraRotationSpeed * RAD2DEG;
        if (IsKeyDown(KEY_UP)) galaxyPitch += kCameraRotationSpeed * RAD2DEG;
        if (IsKeyDown(KEY_RIGHT)) galaxyYaw += kCameraRotationSpeed * RAD2DEG;
        if (IsKeyDown(KEY_LEFT)) galaxyYaw -= kCameraRotationSpeed * RAD2DEG;

        if (!IsGamepadAvailable(0)) {
            bool middleDown = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
            Vector2 delta = GetMouseDelta();
#if defined(__EMSCRIPTEN__)
            middleDown = webMouse.middleDown;
            delta = webMouse.delta;
            webMouse.delta = Vector2 {};
#endif
            if (middleDown) {
                galaxyYaw += delta.x * kCameraMouseSensitivity * RAD2DEG;
                galaxyPitch -= delta.y * kCameraMouseSensitivity * RAD2DEG;
            }
        } else {
            galaxyYaw += GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) *
                         2.0f * kCameraMouseSensitivity * RAD2DEG;
            galaxyPitch -= GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) *
                           2.0f * kCameraMouseSensitivity * RAD2DEG;
        }
        galaxyYaw = std::fmod(galaxyYaw, 360.0f);
        if (galaxyYaw < 0.0f) galaxyYaw += 360.0f;
        galaxyPitch = std::clamp(galaxyPitch, kGalaxyCameraPitchMin, kGalaxyCameraPitchMax);
        ApplyGalaxyCamera(*camera, galaxyYaw, galaxyPitch);
        return;
    }

    bool rotateUp = false;
    if (IsKeyDown(KEY_DOWN)) CameraPitch(camera, -kCameraRotationSpeed, false, false, rotateUp);
    if (IsKeyDown(KEY_UP)) CameraPitch(camera, kCameraRotationSpeed, false, false, rotateUp);
    if (IsKeyDown(KEY_RIGHT)) CameraYaw(camera, -kCameraRotationSpeed, false);
    if (IsKeyDown(KEY_LEFT)) CameraYaw(camera, kCameraRotationSpeed, false);
    if (IsKeyDown(KEY_Q)) CameraRoll(camera, -kCameraRotationSpeed);
    if (IsKeyDown(KEY_E)) CameraRoll(camera, kCameraRotationSpeed);

    if (!IsGamepadAvailable(0)) {
        bool middleDown = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
        bool rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        Vector2 delta = GetMouseDelta();
#if defined(__EMSCRIPTEN__)
        middleDown = webMouse.middleDown;
        rightDown = webMouse.rightDown;
        delta = webMouse.delta;
        webMouse.delta = Vector2 {};
#endif
        if (middleDown) {
            CameraYaw(camera, -delta.x * kCameraMouseSensitivity, true);
            CameraPitch(camera, -delta.y * kCameraMouseSensitivity, true, true, rotateUp);
        } else if (rightDown) {
            CameraRoll(camera, delta.x * kCameraMouseSensitivity);
        }
        if (IsKeyDown(KEY_W)) CameraMoveForward(camera, kCameraMoveSpeed, false);
        if (IsKeyDown(KEY_A)) CameraMoveRight(camera, -kCameraMoveSpeed, false);
        if (IsKeyDown(KEY_S)) CameraMoveForward(camera, -kCameraMoveSpeed, false);
        if (IsKeyDown(KEY_D)) CameraMoveRight(camera, kCameraMoveSpeed, false);
    } else {
        CameraYaw(camera, -GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 2.0f * kCameraMouseSensitivity, false);
        CameraPitch(camera, -GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 2.0f * kCameraMouseSensitivity,
                    false, false, rotateUp);
        if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) <= -0.25f) CameraMoveForward(camera, kCameraMoveSpeed, false);
        if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) <= -0.25f) CameraMoveRight(camera, -kCameraMoveSpeed, false);
        if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) >= 0.25f) CameraMoveForward(camera, -kCameraMoveSpeed, false);
        if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) >= 0.25f) CameraMoveRight(camera, kCameraMoveSpeed, false);
    }
    if (IsKeyDown(KEY_SPACE)) CameraMoveUp(camera, kCameraMoveSpeed);
    if (IsKeyDown(KEY_LEFT_CONTROL)) CameraMoveUp(camera, -kCameraMoveSpeed);
    CameraMoveToTarget(camera, -GetMouseWheelMove());
    if (IsKeyPressed(KEY_KP_SUBTRACT)) CameraMoveToTarget(camera, 2.0f);
    if (IsKeyPressed(KEY_KP_ADD)) CameraMoveToTarget(camera, -2.0f);
}

void SetShaderVector3(Shader shader, const char *name, Vector3 value) {
    int location = GetShaderLocation(shader, name);
    if (location >= 0) SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC3);
}

void SetShaderFloat(Shader shader, const char *name, float value) {
    int location = GetShaderLocation(shader, name);
    if (location >= 0) SetShaderValue(shader, location, &value, SHADER_UNIFORM_FLOAT);
}

void SetShaderVector4(Shader shader, const char *name, Vector4 value) {
    int location = GetShaderLocation(shader, name);
    if (location >= 0) SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC4);
}

void SetShaderMatrix(Shader shader, const char *name, Matrix value) {
    int location = GetShaderLocation(shader, name);
    if (location >= 0) SetShaderValueMatrix(shader, location, value);
}

void ConfigureStarShader(Shader &shader, bool hasNormal, bool hasTexcoord) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "in_POSITION0");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] = hasNormal ? GetShaderLocationAttrib(shader, "in_NORMAL0") : -1;
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = hasTexcoord ? GetShaderLocationAttrib(shader, "in_TEXCOORD0") : -1;
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_MVP] = -1;
}

void ConfigureGalaxyStarPointShader(Shader &shader) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_VERTEX_COLOR] = GetShaderLocationAttrib(shader, "vertexColor");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_MVP] = -1;
    shader.locs[SHADER_LOC_COLOR_DIFFUSE] = GetShaderLocation(shader, "colDiffuse");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "texture0");
}

void ConfigureGalaxyLineShader(Shader &shader) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = -1;
    shader.locs[SHADER_LOC_MATRIX_MVP] = -1;
    shader.locs[SHADER_LOC_COLOR_DIFFUSE] = -1;
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "texture0");
}

void DrawGalaxyLineQuad(Vector3 start, Vector3 end, const Camera3D &camera, float width) {
    Vector3 forward = Vector3Normalize(camera.target - camera.position);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 up = Vector3CrossProduct(right, forward);
    Vector3 segment = end - start;
    Vector3 endRay = end - camera.position;
    Vector3 rayView {
        Vector3DotProduct(endRay, right),
        Vector3DotProduct(endRay, up),
        -Vector3DotProduct(endRay, forward),
    };
    Vector3 segmentView {
        Vector3DotProduct(segment, right),
        Vector3DotProduct(segment, up),
        -Vector3DotProduct(segment, forward),
    };
    auto clampRayComponent = [](float value) {
        return std::copysign(std::max(std::abs(value), 0.01f), value);
    };
    rayView.x = clampRayComponent(rayView.x);
    rayView.y = clampRayComponent(rayView.y);
    rayView.z = clampRayComponent(rayView.z);
    float sideX = rayView.z * segmentView.y - rayView.y * segmentView.z;
    float sideY = rayView.x * segmentView.z - rayView.z * segmentView.x;
    float sideLengthSquared = sideX * sideX + sideY * sideY;
    if (sideLengthSquared < 1.0e-8f) return;
    float sideScale = width * 0.5f / std::sqrt(sideLengthSquared);
    Vector3 side = right * (sideX * sideScale) + up * (sideY * sideScale);
    rlBegin(RL_QUADS);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(start.x - side.x, start.y - side.y, start.z - side.z);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(end.x - side.x, end.y - side.y, end.z - side.z);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(end.x + side.x, end.y + side.y, end.z + side.z);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(start.x + side.x, start.y + side.y, start.z + side.z);
    rlEnd();
}

void ConfigureScreenShader(Shader &shader) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "texture0");
}

RenderTexture2D LoadHalfFloatRenderTexture(int width, int height, bool withDepth) {
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (target.id == 0) return {};
    rlEnableFramebuffer(target.id);
    target.texture.id = rlLoadTexture(nullptr, width, height, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.mipmaps = 1;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (withDepth) {
        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.mipmaps = 1;
        target.depth.format = 19;
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }
    bool complete = rlFramebufferComplete(target.id);
    rlDisableFramebuffer();
    if (!complete || !IsTextureValid(target.texture)) {
        UnloadRenderTexture(target);
        return {};
    }
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(target.texture, TEXTURE_WRAP_CLAMP);
    return target;
}

RenderTexture2D LoadMultisampleHalfFloatRenderTexture(int width, int height, int requestedSamples) {
    RenderTexture2D target{};
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    int samples = 0;
    for (int candidate: {requestedSamples, 4, 2, 1}) {
        if (candidate > maxSamples) continue;
        samples = candidate;
        GLenum previousError = glGetError();
        if (previousError != GL_NO_ERROR) {
            TraceLog(LOG_WARNING, "FBO: OpenGL error before multisample target creation: 0x%04X", previousError);
            return {};
        }
        target.id = rlLoadFramebuffer();
        if (target.id == 0) return {};
        glBindFramebuffer(GL_FRAMEBUFFER, target.id);

        glGenRenderbuffers(1, &target.texture.id);
        glBindRenderbuffer(GL_RENDERBUFFER, target.texture.id);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target.texture.id);

        glGenRenderbuffers(1, &target.depth.id);
        glBindRenderbuffer(GL_RENDERBUFFER, target.depth.id);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depth.id);

        bool complete = glGetError() == GL_NO_ERROR &&
                        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (complete) break;

        if (target.texture.id != 0) glDeleteRenderbuffers(1, &target.texture.id);
        if (target.depth.id != 0) glDeleteRenderbuffers(1, &target.depth.id);
        GLuint framebuffer = target.id;
        glDeleteFramebuffers(1, &framebuffer);
        target = {};
    }
    if (target.id == 0) return {};

    target.texture.width = width;
    target.texture.height = height;
    target.texture.mipmaps = 1;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    target.depth.width = width;
    target.depth.height = height;
    target.depth.mipmaps = 1;
    target.depth.format = 19;
    if (samples < requestedSamples)
        TraceLog(LOG_WARNING, "FBO: Requested %i multisample HDR samples, using %i", requestedSamples, samples);
    TraceLog(LOG_INFO, "FBO: [ID %i] Multisample HDR target loaded successfully (%ix%i | %i samples)",
             target.id, width, height, samples);
    return target;
}

void UnloadMultisampleRenderTexture(RenderTexture2D &target) {
    if (target.texture.id != 0) glDeleteRenderbuffers(1, &target.texture.id);
    if (target.depth.id != 0) glDeleteRenderbuffers(1, &target.depth.id);
    if (target.id != 0) {
        GLuint framebuffer = target.id;
        glDeleteFramebuffers(1, &framebuffer);
    }
    target = {};
}

void BeginMultisampleTextureMode(const RenderTexture2D &target) {
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(target.id);
    rlViewport(0, 0, target.texture.width, target.texture.height);
    rlSetFramebufferWidth(target.texture.width);
    rlSetFramebufferHeight(target.texture.height);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, target.texture.width, target.texture.height, 0, 0.0f, 1.0f);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void ResolveMultisampleTexture(const RenderTexture2D &source, const RenderTexture2D &destination) {
    rlDrawRenderBatchActive();
    rlBindFramebuffer(RL_READ_FRAMEBUFFER, source.id);
    rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, destination.id);
    rlBlitFramebuffer(0, 0, source.texture.width, source.texture.height,
                      0, 0, destination.texture.width, destination.texture.height, GL_COLOR_BUFFER_BIT);
    rlDisableFramebuffer();
    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
    rlSetFramebufferWidth(GetRenderWidth());
    rlSetFramebufferHeight(GetRenderHeight());
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, GetRenderWidth(), GetRenderHeight(), 0, 0.0f, 1.0f);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void UnloadBloomTargets() {
    UnloadMultisampleRenderTexture(galaxySceneMultisampleTarget);
    if (galaxySceneTarget.id != 0) UnloadRenderTexture(galaxySceneTarget);
    if (galaxyFxaaTarget.id != 0) UnloadRenderTexture(galaxyFxaaTarget);
    if (bloomPrefilterTarget.id != 0) UnloadRenderTexture(bloomPrefilterTarget);
    for (auto &level: bloomDownLevels) {
        if (level.id != 0) UnloadRenderTexture(level);
        level = {};
    }
    for (auto &level: bloomUpLevels) {
        if (level.id != 0) UnloadRenderTexture(level);
        level = {};
    }
    galaxySceneMultisampleTarget = {};
    galaxySceneTarget = {};
    galaxyFxaaTarget = {};
    bloomPrefilterTarget = {};
    bloomTargetWidth = 0;
    bloomTargetHeight = 0;
    bloomIterationCount = 0;
    bloomSampleScale = 1.0f;
}

bool EnsureBloomTargets(int width, int height) {
    if (bloomTargetWidth == width && bloomTargetHeight == height && galaxySceneMultisampleTarget.id != 0 &&
        galaxySceneTarget.id != 0 &&
        galaxyFxaaTarget.id != 0 && bloomPrefilterTarget.id != 0 && bloomDownLevels[0].id != 0) return true;
    UnloadBloomTargets();
    galaxySceneMultisampleTarget = LoadMultisampleHalfFloatRenderTexture(width, height, 8);
    galaxySceneTarget = LoadHalfFloatRenderTexture(width, height, true);
    galaxyFxaaTarget = LoadHalfFloatRenderTexture(width, height, false);
    int levelWidth = std::max(1, width / 2);
    int levelHeight = std::max(1, height / 2);
    bloomPrefilterTarget = LoadHalfFloatRenderTexture(levelWidth, levelHeight, false);
    float pyramid = std::log2(static_cast<float>(levelHeight)) + 5.0f - 8.0f;
    int pyramidInteger = static_cast<int>(pyramid);
    bloomIterationCount = std::clamp(pyramidInteger, 1, kMaxBloomIterations);
    bloomSampleScale = 0.5f + pyramid - static_cast<float>(pyramidInteger);
    for (int i = 0; i < bloomIterationCount; ++i) {
        levelWidth = std::max(1, levelWidth / 2);
        levelHeight = std::max(1, levelHeight / 2);
        bloomDownLevels[static_cast<std::size_t>(i)] = LoadHalfFloatRenderTexture(levelWidth, levelHeight, false);
    }
    for (int i = bloomIterationCount - 2; i >= 0; --i) {
        const auto &down = bloomDownLevels[static_cast<std::size_t>(i)];
        bloomUpLevels[static_cast<std::size_t>(i)] =
            LoadHalfFloatRenderTexture(down.texture.width, down.texture.height, false);
    }
    bool complete = galaxySceneMultisampleTarget.id != 0 && galaxySceneTarget.id != 0 &&
                    galaxyFxaaTarget.id != 0 && bloomPrefilterTarget.id != 0;
    for (int i = 0; i < bloomIterationCount; ++i)
        complete = complete && bloomDownLevels[static_cast<std::size_t>(i)].id != 0;
    for (int i = 0; i < bloomIterationCount - 1; ++i)
        complete = complete && bloomUpLevels[static_cast<std::size_t>(i)].id != 0;
    if (!complete) {
        UnloadBloomTargets();
        return false;
    }
    bloomTargetWidth = width;
    bloomTargetHeight = height;
    return true;
}

void BeginOpaqueBlit() {
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlDisableColorBlend();
}

void EndOpaqueBlit() {
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableDepthTest();
}

void DrawFlippedTexture(Texture2D texture, int width, int height) {
    DrawTexturePro(texture,
                   Rectangle {0.0f, 0.0f, static_cast<float>(texture.width), -static_cast<float>(texture.height)},
                   Rectangle {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)},
                   Vector2 {}, 0.0f, WHITE);
}

void SetSourceTexelSize(Shader shader, const Texture2D &texture, float scale = 1.0f) {
    Vector2 texelSize {scale / static_cast<float>(texture.width), scale / static_cast<float>(texture.height)};
    int location = GetShaderLocation(shader, "sourceTexelSize");
    if (location >= 0) SetShaderValue(shader, location, &texelSize, SHADER_UNIFORM_VEC2);
}

template<typename T>
T *LoadMeshArray(const char *path, int expectedCount) {
    int size = 0;
    unsigned char *data = LoadFileData(path, &size);
    if (data == nullptr || size != expectedCount * static_cast<int>(sizeof(T))) {
        if (data != nullptr) UnloadFileData(data);
        return nullptr;
    }
    T *result = static_cast<T *>(MemAlloc(static_cast<unsigned int>(size)));
    if (result == nullptr) {
        UnloadFileData(data);
        return nullptr;
    }
    std::memcpy(result, data, static_cast<std::size_t>(size));
    UnloadFileData(data);
    return result;
}

Model LoadStarModel(const char *verticesPath, int vertexCount, const char *indicesPath, int triangleCount,
                    const char *normalsPath = nullptr, const char *texcoordsPath = nullptr) {
    Mesh mesh{};
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;
    mesh.vertices = LoadMeshArray<float>(verticesPath, vertexCount * 3);
    mesh.indices = LoadMeshArray<unsigned short>(indicesPath, triangleCount * 3);
    if (normalsPath != nullptr) mesh.normals = LoadMeshArray<float>(normalsPath, vertexCount * 3);
    if (texcoordsPath != nullptr) mesh.texcoords = LoadMeshArray<float>(texcoordsPath, vertexCount * 2);
    if (mesh.vertices == nullptr || mesh.indices == nullptr || (normalsPath != nullptr && mesh.normals == nullptr) ||
        (texcoordsPath != nullptr && mesh.texcoords == nullptr)) {
        UnloadMesh(mesh);
        return {};
    }
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}

void SetModelShader(Model &model, Shader shader) {
    model.materials[0].shader = shader;
    for (int i = 0; i <= MATERIAL_MAP_BRDF; ++i) model.materials[0].maps[i].texture = {};
}

bool StarModelReady(const Model &model) {
    return IsModelValid(model) && model.meshCount == 1 && model.materialCount == 1;
}

float StarViewRadius(const dspugen::Star *star) {
    float viewRadius = star->radius * 800.0f;
    return static_cast<float>(static_cast<double>(viewRadius) * 0.00025);
}

float RawStarSpotLevel(const StarViewData &star, const Camera3D &camera) {
    float ratio = Vector3Distance(star.position, camera.position) / std::max(0.0001f, StarViewRadius(star.data));
    return std::max(0.0f, (ratio - 15.0f) * 0.05f);
}

float StarSpotLevel(const StarViewData &star, const Camera3D &camera) {
    float level = RawStarSpotLevel(star, camera);
    if (level > 1.0f) {
        level = std::log(level) + 1.0f;
        level = std::log(level) + 1.0f;
    }
    return std::min(level, 2.0f);
}

float StarVisualScale(const StarViewData &star, const Camera3D &camera) {
    float vradius = StarViewRadius(star.data);
    float vdist = Vector3Distance(star.position, camera.position);
    float factor = static_cast<float>(std::pow(static_cast<double>(std::max(1.0f, star.data->radius)),
                                               0.15000000596046448));
    float minimumScaleRatio = (vdist / std::max(0.0001f, vradius)) / (160.0f / factor);
    float minimumScale = std::max(1.0f, static_cast<float>(std::pow(static_cast<double>(minimumScaleRatio), 0.7)));
    return minimumScale * vradius * 2.0f;
}

void DrawStarSpot(const StarViewData &star, const Camera3D &camera, float visualScale);

Matrix ViewProjection() {
    return MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

Matrix StarViewProjection() {
    Matrix result = ViewProjection();
    result.m2 = (result.m2 + result.m3) * 0.5f;
    result.m6 = (result.m6 + result.m7) * 0.5f;
    result.m10 = (result.m10 + result.m11) * 0.5f;
    result.m14 = (result.m14 + result.m15) * 0.5f;
    return result;
}

void SetStarMatrices(Shader shader, Matrix transform) {
    SetShaderMatrix(shader, "matModel", transform);
    SetShaderMatrix(shader, "matViewProjection", StarViewProjection());
}

Matrix StarTransform(Vector3 position, float scale) {
    return MatrixMultiply(MatrixScale(scale, scale, scale), MatrixTranslate(position.x, position.y, position.z));
}

void SetGalaxyPointUniforms(float pointGlow, float multiplier, float alphaMultiplier) {
    int glowLocation = GetShaderLocation(galaxyStarPointShader, "_PointGlow");
    int multiplierLocation = GetShaderLocation(galaxyStarPointShader, "_Multiplier");
    int alphaLocation = GetShaderLocation(galaxyStarPointShader, "_AlphaMultiplier");
    if (glowLocation >= 0) SetShaderValue(galaxyStarPointShader, glowLocation, &pointGlow, SHADER_UNIFORM_FLOAT);
    if (multiplierLocation >= 0) SetShaderValue(galaxyStarPointShader, multiplierLocation, &multiplier, SHADER_UNIFORM_FLOAT);
    if (alphaLocation >= 0) SetShaderValue(galaxyStarPointShader, alphaLocation, &alphaMultiplier, SHADER_UNIFORM_FLOAT);
}

void DrawStarMass(const StarViewData &star, const Camera3D &camera, float time) {
    float rawSpotLevel = RawStarSpotLevel(star, camera);
    float bodyLevel = std::sqrt(std::clamp(1.0f - rawSpotLevel, 0.0f, 1.0f));
    if (bodyLevel <= 0.0f || !hasStarSurfaceShader || !hasStarModels) return;
    float visualScale = StarVisualScale(star, camera);
    Matrix transform = StarTransform(star.position, visualScale);
    auto material = StarMassMaterial(star.data);
    rlDrawRenderBatchActive();
    SetShaderVector3(starSurfaceShader, "_Color0_xyz", Vector3 {material.color0.x, material.color0.y, material.color0.z});
    SetShaderVector3(starSurfaceShader, "_Color1_xyz", Vector3 {material.color1.x, material.color1.y, material.color1.z});
    SetShaderVector3(starSurfaceShader, "_Color2_xyz", Vector3 {material.color2.x, material.color2.y, material.color2.z});
    SetShaderVector3(starSurfaceShader, "_Color3_xyz", Vector3 {material.color3.x, material.color3.y, material.color3.z});
    SetShaderVector4(starSurfaceShader, "_Multiplier_and_NoiseTile_and_NoiseSpeed_and_ChaosTile",
                     Vector4 {4.0f * bodyLevel, material.noiseTile, material.noiseSpeed, material.chaosTile});
    SetShaderVector4(starSurfaceShader, "_ChaosDistort_and_ChaosOverlay_and_SpotIntens_and_RimPower",
                     Vector4 {material.chaosDistort, material.chaosOverlay, material.spotIntensity, material.rimPower});
    SetShaderVector4(starSurfaceShader, "_RotSpeed", Vector4 {material.rotSpeed, 0.0f, 0.0f, 0.0f});
    SetShaderVector4(starSurfaceShader, "_Time", Vector4 {time / 20.0f, time, time * 2.0f, time * 3.0f});
    SetShaderVector3(starSurfaceShader, "_WorldSpaceCameraPos", camera.position);
    SetStarMatrices(starSurfaceShader, transform);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    rlSetCullFace(RL_CULL_FACE_BACK);
    rlSetBlendMode(RL_BLEND_ADD_COLORS);
    DrawMesh(starMassModel.meshes[0], starMassModel.materials[0], transform);
    rlSetBlendMode(RL_BLEND_ALPHA);
}

void DrawStarBlack(const StarViewData &star, const Camera3D &camera) {
    if (RawStarSpotLevel(star, camera) >= 1.0f || !hasStarBlackShader || !hasStarModels) return;
    float visualScale = StarVisualScale(star, camera);
    Matrix transform = StarTransform(star.position, visualScale * 1.0049999952316284f);
    rlDrawRenderBatchActive();
    SetShaderVector4(starBlackShader, "_TintColor", Vector4 {0.0f, 0.0f, 0.0f, 1.0f});
    SetStarMatrices(starBlackShader, transform);
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    DrawMesh(starBlackModel.meshes[0], starBlackModel.materials[0], transform);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void DrawStarSpot(const StarViewData &star, const Camera3D &camera, float visualScale) {
    if (!hasStarSpotShader || !hasStarModels) return;
    float spotLevel = StarSpotLevel(star, camera);
    if (spotLevel <= 0.0f) return;
    auto material = StarSpotMaterial(star.data);
    rlDrawRenderBatchActive();
    SetShaderVector4(starSpotShader, "_Multiplier", Vector4 {0.3f * spotLevel, 0.0f, 0.0f, 0.0f});
    SetShaderVector3(starSpotShader, "_SunGlowColor_xyz", Vector3 {material.glowColor.x, material.glowColor.y, material.glowColor.z});
    SetShaderVector4(starSpotShader, "_SunGlowPower", Vector4 {material.glowPower, 0.0f, 0.0f, 0.0f});
    SetShaderVector3(starSpotShader, "_SunCenterColor_xyz", Vector3 {material.centerColor.x, material.centerColor.y, material.centerColor.z});
    SetShaderVector4(starSpotShader, "_SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower",
                     Vector4 {material.centerMultiplier, material.centerSize, material.centerBlur, material.centerBlurPower});
    SetShaderVector4(starSpotShader, "_Global_WhiteMode0", Vector4 {});
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 worldUp {0.0f, 1.0f, 0.0f};
    if (std::abs(Vector3DotProduct(forward, worldUp)) > 0.999999f) worldUp = Vector3 {0.0f, 0.0f, 1.0f};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(worldUp, forward));
    Vector3 up = Vector3CrossProduct(forward, right);
    Matrix rotation {right.x, up.x, forward.x, 0.0f,
                     right.y, up.y, forward.y, 0.0f,
                     right.z, up.z, forward.z, 0.0f,
                     0.0f, 0.0f, 0.0f, 1.0f};
    Matrix transform = MatrixMultiply(MatrixScale(visualScale * 3.700000047683716f,
                                                   visualScale * 3.700000047683716f,
                                                   visualScale * 3.700000047683716f), rotation);
    transform = MatrixMultiply(transform, MatrixTranslate(star.position.x, star.position.y, star.position.z));
    SetStarMatrices(starSpotShader, transform);
    rlSetBlendMode(RL_BLEND_ADD_COLORS);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    DrawMesh(starSpotModel.meshes[0], starSpotModel.materials[0], transform);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
}

void DrawGalaxyStarPoint(const StarViewData &star, const Camera3D &camera) {
    if (!hasGalaxyStarPointShader || !hasGalaxyStarPointModel) return;
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 up = Vector3Normalize(camera.up);
    if (std::abs(Vector3DotProduct(forward, up)) > 0.999999f) up = Vector3 {0.0f, 0.0f, 1.0f};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(up, forward));
    up = Vector3CrossProduct(forward, right);
    Matrix rotation {right.x, up.x, forward.x, 0.0f,
                     right.y, up.y, forward.y, 0.0f,
                     right.z, up.z, forward.z, 0.0f,
                     0.0f, 0.0f, 0.0f, 1.0f};
    Matrix transform = MatrixMultiply(MatrixScale(star.galaxyScale, star.galaxyScale, star.galaxyScale), rotation);
    transform = MatrixMultiply(transform, MatrixTranslate(star.position.x, star.position.y, star.position.z));
    rlDrawRenderBatchActive();
    SetShaderMatrix(galaxyStarPointShader, "matViewProjection", ViewProjection());
    SetShaderVector4(galaxyStarPointShader, "_MainTex_ST", Vector4 {1.0f, 1.0f, 0.0f, 0.0f});
    SetShaderVector4(galaxyStarPointShader, "_TintColor", star.galaxyColor);
    int framebufferWidth = rlGetFramebufferWidth();
    int framebufferHeight = rlGetFramebufferHeight();
    SetShaderVector4(galaxyStarPointShader, "_ScreenParams",
                     Vector4 {static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight), 0.0f, 0.0f});
    float fadeRadius = 0.0f;
    int fadeLocation = GetShaderLocation(galaxyStarPointShader, "_Global_GalaxyFadeRadius");
    if (fadeLocation >= 0) SetShaderValue(galaxyStarPointShader, fadeLocation, &fadeRadius, SHADER_UNIFORM_FLOAT);
    float sceneLinear = (galaxySceneTarget.id != 0 && rlGetActiveFramebuffer() == galaxySceneTarget.id) ||
                        (galaxySceneMultisampleTarget.id != 0 &&
                         rlGetActiveFramebuffer() == galaxySceneMultisampleTarget.id) ? 1.0f : 0.0f;
    int sceneLinearLocation = GetShaderLocation(galaxyStarPointShader, "_SceneLinear");
    if (sceneLinearLocation >= 0)
        SetShaderValue(galaxyStarPointShader, sceneLinearLocation, &sceneLinear, SHADER_UNIFORM_FLOAT);
    Vector4 white {1.0f, 1.0f, 1.0f, 1.0f};
    int colorLocation = GetShaderLocation(galaxyStarPointShader, "colDiffuse");
    if (colorLocation >= 0) SetShaderValue(galaxyStarPointShader, colorLocation, &white, SHADER_UNIFORM_VEC4);
    auto &material = galaxyStarPointModel.materials[0];
    int savedColorLocation = material.shader.locs[SHADER_LOC_COLOR_DIFFUSE];
    material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] = -1;
    rlDisableDepthMask();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlColorMask(true, true, true, false);
    SetGalaxyPointUniforms(0.0f, 2.5f, 1.0f);
    rlSetBlendFactors(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM);
    DrawMesh(galaxyStarPointModel.meshes[0], material, transform);
    material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] = savedColorLocation;
    rlColorMask(true, true, true, true);
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
}

void DrawGalaxyLines(const std::vector<GalaxyLineData> &lines,
                     const std::vector<StarViewData> &stars, const Camera3D &camera) {
    if (!hasGalaxyLineShader || !IsTextureValid(galaxyLineTexture)) return;
    rlDrawRenderBatchActive();
    SetShaderMatrix(galaxyLineShader, "matViewProjection", ViewProjection());
    SetShaderVector3(galaxyLineShader, "_WorldSpaceCameraPos", camera.position);
    SetShaderFloat(galaxyLineShader, "_Multiplier", 0.349999994f);
    SetShaderFloat(galaxyLineShader, "_TileScale", 6.0f);
    SetShaderFloat(galaxyLineShader, "_DistScale", 0.0299999993f);
    SetShaderFloat(galaxyLineShader, "_DistPower", 0.899999976f);
    SetShaderFloat(galaxyLineShader, "_MinDistScale", 0.100000001f);
    SetShaderFloat(galaxyLineShader, "_MaxDistScale", 10.0f);
    SetShaderFloat(galaxyLineShader, "_Global_GalaxyFadeRadius", 0.0f);
    float sceneLinear = (galaxySceneTarget.id != 0 && rlGetActiveFramebuffer() == galaxySceneTarget.id) ||
                        (galaxySceneMultisampleTarget.id != 0 &&
                         rlGetActiveFramebuffer() == galaxySceneMultisampleTarget.id) ? 1.0f : 0.0f;
    SetShaderFloat(galaxyLineShader, "_SceneLinear", sceneLinear);
    SetShaderVector4(galaxyLineShader, "_ScreenParams",
                     Vector4 {static_cast<float>(rlGetFramebufferWidth()),
                              static_cast<float>(rlGetFramebufferHeight()), 0.0f, 0.0f});
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetBlendFactors(RL_SRC_ALPHA, RL_ONE, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM);
    BeginShaderMode(galaxyLineShader);
    rlSetTexture(galaxyLineTexture.id);
    for (const auto &line: lines) {
        const auto &startStar = stars[line.start];
        const auto &endStar = stars[line.end];
        Vector3 span = endStar.position - startStar.position;
        float distance = Vector3Length(span);
        if (distance <= 0.0f) continue;
        Vector3 start = startStar.position + span * ((startStar.galaxyScale * 0.25f + 0.35f) / distance);
        Vector3 end = startStar.position + span * (1.0f - (endStar.galaxyScale * 0.25f + 0.35f) / distance);
        SetShaderVector4(galaxyLineShader, "_LineColorA", GalaxyLineColor(startStar.galaxyColor));
        SetShaderVector4(galaxyLineShader, "_LineColorB", GalaxyLineColor(endStar.galaxyColor));
        DrawGalaxyLineQuad(start, end, camera, 1.0f);
        rlDrawRenderBatchActive();
    }
    rlSetTexture(0);
    EndShaderMode();
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void DrawBackgroundStars(const Camera3D &camera, bool bloomVisible = true) {
    if (!bloomVisible) return;
    for (int layer = 0; layer < 2; ++layer) {
        float follow = layer == 0 ? 0.93f : 0.96f;
        float shellRadius = layer == 0 ? 105.0f : 180.0f;
        int count = layer == 0 ? 90 : 150;
        Vector3 center = camera.position * follow;
        for (int i = 0; i < count; ++i) {
            float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            float ring = std::sqrt(std::max(0.0f, 1.0f - y * y));
            float angle = static_cast<float>(i) * 2.39996323f + Hash01(i + layer * 997) * 0.35f;
            Vector3 point = center + Vector3 {std::cos(angle) * ring, y, std::sin(angle) * ring} * shellRadius;
            float size = (layer == 0 ? 0.025f : 0.035f) * (0.65f + Hash01(i * 17 + layer) * 0.9f);
            Color color = Hash01(i * 31 + layer) > 0.82f ? Color {132, 186, 231, 180} : Color {194, 211, 224, 145};
            DrawSphere(point, size, color);
        }
    }
}

void DrawCachedLines(const std::vector<Line3DCache> &lines, Color color) {
    for (const auto &line: lines) DrawLine3D(line.start, line.end, color);
}

Vector3 TransformOrbitPoint(Vector3 point, float inclination, float longitude) {
    Matrix heading = MatrixRotateY(longitude);
    Matrix tilt = MatrixRotateZ(inclination);
    return Vector3Transform(point, MatrixMultiply(heading, tilt));
}

Vector3 OrbitPoint(Vector3 center, float radius, float angle, float inclination, float longitude) {
    return center + TransformOrbitPoint(
        Vector3 {std::cos(angle) * radius, 0.0f, std::sin(angle) * radius}, inclination, longitude);
}

Vector3 OrbitPoint(Vector3 center, float radius, float angle, Matrix rotation) {
    Vector3 point {std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
    return center + Vector3Transform(point, rotation);
}

std::vector<int> ReplayInfoSeeds(const dspugen::Star *star) {
    dspugen::util::DotNet35Random rng(star->seed);
    rng.next();
    rng.next();
    rng.next();
    dspugen::util::DotNet35Random planetRng(rng.next());
    for (int i = 0; i < 7; ++i) planetRng.nextDouble();

    std::vector<int> seeds;
    seeds.reserve(star->planets.size());
    const bool special = star->type != dspugen::EStarType::MainSeqStar;
    if (special) {
        for (std::size_t i = 0; i < star->planets.size(); ++i) {
            seeds.push_back(planetRng.next());
            planetRng.next();
        }
        return seeds;
    }

    int orbitIndex = 1;
    for (std::size_t i = 0; i < star->planets.size(); ++i) {
        seeds.push_back(planetRng.next());
        planetRng.next();
        planetRng.nextDouble();
        planetRng.nextDouble();
        const auto *planet = star->planets[i];
        bool mainPlanet = planet->orbitAround == 0;
        if (mainPlanet) {
            bool gasGiant = planet->type == dspugen::EPlanetType::Gas && i + 1 < star->planets.size();
            if (gasGiant && orbitIndex < 3) orbitIndex = 3;
            while (true) {
                if (star->index == 0 && orbitIndex == 3) break;
                int remaining = static_cast<int>(star->planets.size() - i);
                int slots = 9 - orbitIndex;
                if (slots <= remaining) break;
                float chance = static_cast<float>(remaining) / static_cast<float>(slots);
                chance = orbitIndex <= 3 ? chance + (1.0f - chance) * 0.15f + 0.01f
                                         : chance + (1.0f - chance) * 0.45f + 0.01f;
                if (planetRng.nextDouble() < chance) break;
                ++orbitIndex;
            }
        }
        ++orbitIndex;
    }
    return seeds;
}

OrbitParameters CreateOrbitParameters(const dspugen::Star *star, const dspugen::Planet *planet, int infoSeed) {
    dspugen::util::DotNet35Random rng(infoSeed);
    double num2 = rng.nextDouble();
    double num3 = rng.nextDouble();
    double num4 = rng.nextDouble();
    double num5 = rng.nextDouble();
    double num6 = rng.nextDouble();
    OrbitParameters result;
    result.inclination = static_cast<float>(num4 * 16.0 - 8.0);
    if (planet->orbitAround > 0) result.inclination *= 2.2f;
    result.longitude = static_cast<float>(num5 * 360.0);
    if (star->type >= dspugen::EStarType::NeutronStar)
        result.inclination += result.inclination > 0.0f ? 3.0f : -3.0f;
    result.phase = static_cast<float>(num6 * 360.0);
    double radius = planet->orbitRadius;
    result.period = std::sqrt(39.478417604357432 * radius * radius * radius /
                               (planet->orbitAroundPlanet == nullptr
                                    ? 1.3538551990520382E-06 * star->mass
                                    : 1.0830842106853677E-08));
    (void)num2;
    (void)num3;
    return result;
}

Matrix PlanetOrbitRotation(float inclination, float longitude) {
    return MatrixMultiply(MatrixRotateY(longitude), MatrixRotateZ(inclination));
}

void DrawOrbit(Vector3 center, const PlanetViewData &planet) {
    Vector3 previous = OrbitPoint(center, planet.orbitRadius, 0.0f, planet.orbitRotation);
    for (int i = 1; i <= kOrbitSegments; ++i) {
        float angle = static_cast<float>(i) / kOrbitSegments * 2.0f * PI;
        Vector3 next = OrbitPoint(center, planet.orbitRadius, angle, planet.orbitRotation);
        DrawLine3D(previous, next, Color {67, 119, 150, 122});
        previous = next;
    }
}

void DrawPlanet(const PlanetViewData &planet, bool selected) {
    Color nightColor {
        static_cast<unsigned char>(planet.color.r * 0.13f),
        static_cast<unsigned char>(planet.color.g * 0.15f),
        static_cast<unsigned char>(planet.color.b * 0.2f), 255};
    DrawSphere(planet.position, planet.radius, nightColor);
    DrawSphere(planet.position, planet.radius * 0.995f, planet.color);
    DrawSphereWires(planet.position, planet.radius * 1.006f, 14, 10, WithAlpha(AddColor(planet.color, 45), 0.2f));
    if (selected) {
        DrawSphereWires(planet.position, planet.selectionRadius, 12, 8, WHITE);
    }
}

void TextLine(const char *text, int x, int y, int size, Color color = RAYWHITE) {
    DrawTextEx(viewerFont, text, Vector2 {static_cast<float>(x), static_cast<float>(y)},
               static_cast<float>(size), 1.0f, color);
}

void DrawButton(Rectangle bounds, const char *label, int labelWidth, bool accent = false) {
    Color fill = accent ? Color {32, 101, 151, 235} : Color {19, 31, 48, 235};
    Color border = accent ? Color {88, 198, 255, 255} : Color {72, 104, 133, 255};
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
        fill = AddColor(fill, 22);
        border = WHITE;
    }
    DrawRectangleRounded(bounds, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.0f, border);
    TextLine(label, static_cast<int>(bounds.x + (bounds.width - labelWidth) * 0.5f), static_cast<int>(bounds.y + 9), 16, WHITE);
}

void DrawPanelBackground() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    DrawRectangle(width - static_cast<int>(kPanelWidth), 0, static_cast<int>(kPanelWidth), height, Color {7, 15, 28, 238});
    DrawLine(width - static_cast<int>(kPanelWidth), 0, width - static_cast<int>(kPanelWidth), height, Color {47, 91, 124, 220});
}

int DrawKeyValue(const PanelRow &row, int y) {
    int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
        TextLine(row.key, x, y, 15, Color {123, 157, 183, 255});
        TextLine(row.value.c_str(), x + 112, y, 15, RAYWHITE);
        return y + 25;
}

void DrawPanel(const PanelData &panel) {
    DrawPanelBackground();
    int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
    TextLine(panel.title.c_str(), x, 25, 20, Color {137, 221, 255, 255});
    TextLine(panel.name.c_str(), x, 58, 24, WHITE);
    TextLine(panel.subtitle.c_str(), x, 89, 14, Color {161, 187, 208, 255});
    int y = 126;
    for (const auto &row: panel.rows) y = DrawKeyValue(row, y);
    if (!panel.detailRows.empty()) {
        TextLine("MINERAL VEINS", x, y + 8, 15, Color {123, 157, 183, 255});
        y += 34;
        for (const auto &row: panel.detailRows) {
            TextLine(row.key, x, y, 14, RAYWHITE);
            int valueWidth = static_cast<int>(MeasureTextEx(viewerFont, row.value.c_str(), 14.0f, 1.0f).x);
            TextLine(row.value.c_str(), GetScreenWidth() - 22 - valueWidth, y, 14, Color {137, 221, 255, 255});
            y += 22;
        }
    }
    if (!panel.footerKey.empty()) TextLine(panel.footerKey.c_str(), x, y + 8, 15, Color {123, 157, 183, 255});
    if (!panel.footerValue.empty()) TextLine(panel.footerValue.c_str(), x, y + (panel.footerKey.empty() ? 16 : 31), 14,
                                             panel.footerKey.empty() ? Color {98, 131, 159, 255} : RAYWHITE);
}

class Viewer {
public:
    Viewer(int seed, int starCount, std::string fontPath)
        : seed(seed), starCount(starCount), fontPath(std::move(fontPath)),
          seedInput(std::to_string(seed)), starCountInput(std::to_string(starCount)) {}

    bool Init() {
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(kScreenWidth, kScreenHeight, "DSPSeedCalc Universe Viewer");
        viewerFont = GetFontDefault();
        const auto surfaceVertex = ViewerAssetPath("star_surface.vs");
        const auto surfaceFragment = ViewerAssetPath("star_surface.fs");
        const auto spotVertex = ViewerAssetPath("star_spot.vs");
        const auto spotFragment = ViewerAssetPath("star_spot.fs");
        const auto blackVertex = ViewerAssetPath("star_black.vs");
        const auto blackFragment = ViewerAssetPath("star_black.fs");
        const auto galaxyPointVertex = ViewerAssetPath("galaxy_star_point.vs");
        const auto galaxyPointFragment = ViewerAssetPath("galaxy_star_point.fs");
        const auto galaxyLineVertex = ViewerAssetPath("galaxy_line.vs");
        const auto galaxyLineFragment = ViewerAssetPath("galaxy_line.fs");
        const auto bloomVertex = ViewerAssetPath("bloom.vs");
        const auto bloomPrefilterFragment = ViewerAssetPath("bloom_prefilter.fs");
        const auto bloomDownsampleFirstFragment = ViewerAssetPath("bloom_downsample_first.fs");
        const auto bloomDownsampleFragment = ViewerAssetPath("bloom_downsample.fs");
        const auto bloomUpsampleFragment = ViewerAssetPath("bloom_upsample.fs");
        const auto bloomCompositeFragment = ViewerAssetPath("bloom_composite.fs");
        const auto galaxyFxaaFragment = ViewerAssetPath("galaxy_fxaa.fs");
        starSurfaceShader = LoadShader(surfaceVertex.c_str(), surfaceFragment.c_str());
        starSpotShader = LoadShader(spotVertex.c_str(), spotFragment.c_str());
        starBlackShader = LoadShader(blackVertex.c_str(), blackFragment.c_str());
        galaxyStarPointShader = LoadShader(galaxyPointVertex.c_str(), galaxyPointFragment.c_str());
        galaxyLineShader = LoadShader(galaxyLineVertex.c_str(), galaxyLineFragment.c_str());
        bloomPrefilterShader = LoadShader(bloomVertex.c_str(), bloomPrefilterFragment.c_str());
        bloomDownsampleFirstShader = LoadShader(bloomVertex.c_str(), bloomDownsampleFirstFragment.c_str());
        bloomDownsampleShader = LoadShader(bloomVertex.c_str(), bloomDownsampleFragment.c_str());
        bloomUpsampleShader = LoadShader(bloomVertex.c_str(), bloomUpsampleFragment.c_str());
        bloomCompositeShader = LoadShader(bloomVertex.c_str(), bloomCompositeFragment.c_str());
        galaxyFxaaShader = LoadShader(bloomVertex.c_str(), galaxyFxaaFragment.c_str());
        hasStarSurfaceShader = IsShaderValid(starSurfaceShader) && starSurfaceShader.id != rlGetShaderIdDefault();
        hasStarSpotShader = IsShaderValid(starSpotShader) && starSpotShader.id != rlGetShaderIdDefault();
        hasStarBlackShader = IsShaderValid(starBlackShader) && starBlackShader.id != rlGetShaderIdDefault();
        hasGalaxyStarPointShader = IsShaderValid(galaxyStarPointShader) &&
                                   galaxyStarPointShader.id != rlGetShaderIdDefault();
        hasGalaxyLineShader = IsShaderValid(galaxyLineShader) && galaxyLineShader.id != rlGetShaderIdDefault();
        hasBloomShaders = IsShaderValid(bloomPrefilterShader) && IsShaderValid(bloomDownsampleFirstShader) &&
                          IsShaderValid(bloomDownsampleShader) && IsShaderValid(bloomUpsampleShader) &&
                          IsShaderValid(bloomCompositeShader) &&
                          bloomPrefilterShader.id != rlGetShaderIdDefault() &&
                          bloomDownsampleFirstShader.id != rlGetShaderIdDefault() &&
                          bloomDownsampleShader.id != rlGetShaderIdDefault() &&
                          bloomUpsampleShader.id != rlGetShaderIdDefault() &&
                          bloomCompositeShader.id != rlGetShaderIdDefault();
        hasGalaxyFxaaShader = IsShaderValid(galaxyFxaaShader) && galaxyFxaaShader.id != rlGetShaderIdDefault();
        if (hasStarSurfaceShader) ConfigureStarShader(starSurfaceShader, true, false);
        if (hasStarSpotShader) ConfigureStarShader(starSpotShader, false, true);
        if (hasStarBlackShader) ConfigureStarShader(starBlackShader, false, false);
        if (hasGalaxyStarPointShader) ConfigureGalaxyStarPointShader(galaxyStarPointShader);
        if (hasGalaxyLineShader) ConfigureGalaxyLineShader(galaxyLineShader);
        if (hasBloomShaders) {
            ConfigureScreenShader(bloomPrefilterShader);
            ConfigureScreenShader(bloomDownsampleFirstShader);
            ConfigureScreenShader(bloomDownsampleShader);
            ConfigureScreenShader(bloomUpsampleShader);
            ConfigureScreenShader(bloomCompositeShader);
        }
        if (hasGalaxyFxaaShader) ConfigureScreenShader(galaxyFxaaShader);
        const auto massVertices = ViewerAssetPath("star_mass_vertices.bin");
        const auto massNormals = ViewerAssetPath("star_mass_normals.bin");
        const auto massIndices = ViewerAssetPath("star_mass_indices.bin");
        const auto blackVertices = ViewerAssetPath("star_black_vertices.bin");
        const auto blackIndices = ViewerAssetPath("star_black_indices.bin");
        const auto spotVertices = ViewerAssetPath("star_spot_vertices.bin");
        const auto spotTexcoords = ViewerAssetPath("star_spot_uv.bin");
        const auto spotIndices = ViewerAssetPath("star_spot_indices.bin");
        const auto galaxyPointVertices = ViewerAssetPath("galaxy_star_point_vertices.bin");
        const auto galaxyPointTexcoords = ViewerAssetPath("galaxy_star_point_uv.bin");
        const auto galaxyPointIndices = ViewerAssetPath("galaxy_star_point_indices.bin");
        const auto galaxyPointTexturePath = ViewerAssetPath("galaxy_star_point.png");
        const auto galaxyLineTexturePath = ViewerAssetPath("galaxy_line_texture.bin");
        const auto bloomLensDirtPath = ViewerAssetPath("LensDirt00.png");
        const auto galaxyColorGradingLutPath = ViewerAssetPath("galaxy_select_lut.bin");
        starMassModel = LoadStarModel(massVertices.c_str(), 2617, massIndices.c_str(), 5120, massNormals.c_str());
        starBlackModel = LoadStarModel(blackVertices.c_str(), 515, blackIndices.c_str(), 768);
        starSpotModel = LoadStarModel(spotVertices.c_str(), 4, spotIndices.c_str(), 2, nullptr, spotTexcoords.c_str());
        galaxyStarPointModel = LoadStarModel(galaxyPointVertices.c_str(), 4, galaxyPointIndices.c_str(), 2,
                                             nullptr, galaxyPointTexcoords.c_str());
        galaxyStarPointTexture = LoadTexture(galaxyPointTexturePath.c_str());
        galaxyLineTexture = LoadGalaxyLineTexture(galaxyLineTexturePath.c_str());
        bloomLensDirtTexture = LoadTexture(bloomLensDirtPath.c_str());
        Image gradingLutImage = LoadImageRaw(galaxyColorGradingLutPath.c_str(), 1024, 32,
                                             PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 0);
        if (IsImageValid(gradingLutImage)) {
            galaxyColorGradingLut = LoadTextureFromImage(gradingLutImage);
            UnloadImage(gradingLutImage);
        }
        hasStarModels = StarModelReady(starMassModel) && StarModelReady(starBlackModel) && StarModelReady(starSpotModel) &&
                        hasStarSurfaceShader && hasStarSpotShader && hasStarBlackShader;
        hasGalaxyStarPointModel = StarModelReady(galaxyStarPointModel) && IsTextureValid(galaxyStarPointTexture) &&
                                  hasGalaxyStarPointShader;
        if (!hasStarModels || !hasGalaxyStarPointModel || !hasGalaxyLineShader || !hasBloomShaders ||
            !hasGalaxyFxaaShader ||
            !IsTextureValid(galaxyLineTexture) ||
            !IsTextureValid(bloomLensDirtTexture) || !IsTextureValid(galaxyColorGradingLut)) {
            Shutdown();
            return false;
        }
        SetModelShader(starMassModel, starSurfaceShader);
        SetModelShader(starBlackModel, starBlackShader);
        SetModelShader(starSpotModel, starSpotShader);
        SetModelShader(galaxyStarPointModel, galaxyStarPointShader);
        galaxyStarPointModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = galaxyStarPointTexture;
        SetTextureFilter(galaxyStarPointTexture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(galaxyStarPointTexture, TEXTURE_WRAP_CLAMP);
        SetTextureFilter(galaxyLineTexture, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(galaxyLineTexture, TEXTURE_WRAP_CLAMP);
        SetTextureFilter(bloomLensDirtTexture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(bloomLensDirtTexture, TEXTURE_WRAP_CLAMP);
        SetTextureFilter(galaxyColorGradingLut, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(galaxyColorGradingLut, TEXTURE_WRAP_CLAMP);
        std::string resolvedFontPath = fontPath;
        if (resolvedFontPath == DSP_VIEWER_DEFAULT_FONT && !resolvedFontPath.empty())
            resolvedFontPath = ViewerAssetPath(resolvedFontPath.c_str());
        if (!resolvedFontPath.empty() && FileExists(resolvedFontPath.c_str())) {
            Font loadedFont = LoadFontEx(resolvedFontPath.c_str(), kViewerFontSize, nullptr, 0);
            if (loadedFont.glyphCount > 0) {
                viewerFont = loadedFont;
                hasCustomFont = true;
                SetTextureFilter(viewerFont.texture, TEXTURE_FILTER_BILINEAR);
            }
        }
        SetTargetFPS(60);
#if defined(__EMSCRIPTEN__)
        InitWebMouse();
#endif

        dspugen::settings.hasPlanets = true;
        dspugen::settings.birthOnly = false;
        dspugen::settings.genName = true;
        dspugen::settings.noPosition = false;
#if !defined(__EMSCRIPTEN__)
        if (!ChangeDirectory(GetApplicationDirectory())) {
            Shutdown();
            return false;
        }
#endif
        dspugen::loadProtoSets();
        dspugen::Galaxy::initThread();
        dspugen::Star::initThread();
        dspugen::Planet::initThread();
        generatorThreadsInitialized = true;
        if (RebuildGalaxy()) return true;
        Shutdown();
        return false;
    }

    bool RebuildGalaxy() {
        if (galaxy != nullptr) {
            galaxy->release();
            galaxy = nullptr;
        }
        stars.clear();
        systems.clear();
        galaxyLines.clear();
        galaxyGridLines.clear();
        selectedStar = nullptr;
        hoveredStar = nullptr;
        selectedPlanet = nullptr;
        hoveredPlanet = nullptr;
        previousClickedStar = nullptr;
        previousClickedPlanet = nullptr;
        previousClickTime = -1.0;
        mode = ViewMode::Galaxy;
        simulationTime = 0.0;
        lastFrameTime = GetTime();

        galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, seed, starCount);
        if (galaxy == nullptr) return false;
        galaxy->ensureAllGas();

        stars.reserve(galaxy->stars.size());
        systems.resize(galaxy->stars.size());
        for (const auto *star: galaxy->stars) {
            Vector3 position {
                static_cast<float>(star->position.x),
                static_cast<float>(star->position.y),
                -static_cast<float>(star->position.z),
            };
            StarViewData view;
            view.data = star;
            view.position = position;
            view.galaxyColor = GalaxyStarColor(star);
            view.galaxyScale = GalaxyStarScale(star);
            view.radius = view.galaxyScale * 0.5f;
            view.selectionSize = Vector3 {view.galaxyScale, view.galaxyScale, view.galaxyScale};
            view.labelWidth = MeasureTextEx(viewerFont, star->name.c_str(), 16.0f, 1.0f).x;
            view.panel = CreateStarPanel(star, seed);
            stars.emplace_back(std::move(view));
        }
        galaxyLines = CreateGalaxyLines(galaxy, stars);
        for (std::size_t i = 0; i < stars.size(); ++i) BuildSystemCache(i);
        galaxySummary = FormatString("seed %d  /  %d stars  /  birth star #%d", seed, galaxy->starCount, galaxy->birthStarId);
        galaxyGridLines.reserve(36);
        constexpr int gridSlices = 8;
        constexpr float gridSpacing = 10.0f;
        float halfGrid = gridSlices * 0.5f * gridSpacing;
        for (int i = -gridSlices / 2; i <= gridSlices / 2; ++i) {
            float offset = static_cast<float>(i) * gridSpacing;
            galaxyGridLines.push_back(Line3DCache {{offset, kGalaxyGridHeight, -halfGrid},
                                                    {offset, kGalaxyGridHeight, halfGrid}});
            galaxyGridLines.push_back(Line3DCache {{-halfGrid, kGalaxyGridHeight, offset},
                                                    {halfGrid, kGalaxyGridHeight, offset}});
        }
        ResetCamera(camera, mode, galaxyCameraYaw, galaxyCameraPitch);
        return true;
    }

    void Run() {
#if defined(__EMSCRIPTEN__)
        current = this;
        emscripten_set_main_loop(Loop, 0, 1);
#else
        while (!WindowShouldClose()) {
            Update();
            Draw();
        }
        Shutdown();
#endif
    }

private:
    static Viewer *current;

    int seed = 0;
    int starCount = 64;
    std::string fontPath;
    dspugen::Galaxy *galaxy = nullptr;
    Camera3D camera{};
    ViewMode mode = ViewMode::Galaxy;
    std::vector<StarViewData> stars;
    std::vector<SystemViewCache> systems;
    std::vector<GalaxyLineData> galaxyLines;
    std::vector<Line3DCache> galaxyGridLines;
    std::string galaxySummary;
    const StarViewData *selectedStar = nullptr;
    const StarViewData *hoveredStar = nullptr;
    const PlanetViewData *selectedPlanet = nullptr;
    const PlanetViewData *hoveredPlanet = nullptr;
    double previousClickTime = -1.0;
    const StarViewData *previousClickedStar = nullptr;
    const PlanetViewData *previousClickedPlanet = nullptr;
    bool shutdown = false;
    bool generatorThreadsInitialized = false;
    double simulationTime = 0.0;
    double lastFrameTime = -1.0;
    float timeScale = 10.0f;
    float galaxyCameraYaw = 0.0f;
    float galaxyCameraPitch = kGalaxyCameraPitchBegin;
    bool draggingTimeSlider = false;
    std::string seedInput;
    std::string starCountInput;
    int activeInput = 0;
    std::size_t inputCursor[2] = {};
    std::size_t inputAnchor[2] = {};
    bool draggingInput = false;
    bool inputConsumed = false;

    static void Loop() {
        if (current == nullptr) return;
        current->Update();
        current->Draw();
    }

    SystemViewCache &CurrentSystem() {
        return systems[static_cast<std::size_t>(selectedStar - stars.data())];
    }

    const SystemViewCache &CurrentSystem() const {
        return systems[static_cast<std::size_t>(selectedStar - stars.data())];
    }

    void BuildSystemCache(std::size_t starIndex) {
        const auto &starView = stars[starIndex];
        auto &system = systems[starIndex];
        const auto infoSeeds = ReplayInfoSeeds(starView.data);
        system.planets.reserve(starView.data->planets.size());
        for (std::size_t planetIndex = 0; planetIndex < starView.data->planets.size(); ++planetIndex) {
            const auto *planet = starView.data->planets[planetIndex];
            Vector3 center {};
            int parentIndex = -1;
            if (planet->orbitAroundPlanet != nullptr) {
                for (std::size_t i = 0; i < system.planets.size(); ++i) {
                    const auto &parent = system.planets[i];
                    if (parent.data == planet->orbitAroundPlanet) {
                        center = parent.position;
                        parentIndex = static_cast<int>(i);
                        break;
                    }
                }
            }
            float orbitRadius = planet->orbitRadius * kSystemOrbitScale;
            auto orbit = CreateOrbitParameters(starView.data, planet, infoSeeds[planetIndex]);
            float inclination = orbit.inclination * DEG2RAD;
            float longitude = orbit.longitude * DEG2RAD;
            float angle = orbit.phase * DEG2RAD;
            Matrix orbitRotation = PlanetOrbitRotation(inclination, longitude);
            if (parentIndex >= 0)
                orbitRotation = MatrixMultiply(system.planets[static_cast<std::size_t>(parentIndex)].orbitRotation,
                                               orbitRotation);
            Vector3 position = OrbitPoint(center, orbitRadius, angle, orbitRotation);
            float radius = planet->realRadius() * 2.0f * 0.00025f;
            PlanetViewData view;
            view.data = planet;
            view.position = position;
            view.radius = radius;
            view.color = PlanetColor(planet->type);
            view.flickerPhase = planet->seed * 0.02f;
            view.selectionRadius = radius * 1.35f;
            view.parentIndex = parentIndex;
            view.orbitRadius = orbitRadius;
            view.orbitInclination = inclination;
            view.orbitLongitude = longitude;
            view.orbitPhase = angle;
            view.orbitalPeriod = std::max(orbit.period, 1.0);
            view.orbitRotation = orbitRotation;
            view.name = PlanetName(planet);
            view.labelWidth = MeasureTextEx(viewerFont, view.name.c_str(), 16.0f, 1.0f).x;
            view.panel = CreatePlanetPanel(planet, view.name);
            system.radius = std::max(system.radius, Vector3Length(position) + radius);
            system.planets.emplace_back(std::move(view));
        }
        system.star = starView;
        system.star.position = Vector3 {};
        system.star.radius = StarViewRadius(starView.data);
        system.star.selectionSize = Vector3 {system.star.radius * 1.8f, system.star.radius * 1.8f,
                                             system.star.radius * 1.8f};
        system.summary = FormatString("system view  /  %d planets  /  click a planet for details",
                                      static_cast<int>(system.planets.size()));
    }

    void UpdatePlanetPositions(double now) {
        if (mode != ViewMode::System || selectedStar == nullptr) return;
        auto &system = CurrentSystem();
        for (auto &planet: system.planets) {
            Vector3 center {};
            if (planet.parentIndex >= 0)
                center = system.planets[static_cast<std::size_t>(planet.parentIndex)].position;
            float angle = planet.orbitPhase +
                          static_cast<float>(simulationTime * 60.0 / planet.orbitalPeriod * 2.0 * PI);
            double phase = static_cast<double>(angle) / (2.0 * PI);
            phase -= std::floor(phase + 0.1);
            planet.position = OrbitPoint(center, planet.orbitRadius,
                                         static_cast<float>(phase * 2.0 * PI), planet.orbitRotation);
        }
        (void)now;
    }

    Rectangle TimeSliderBounds() const {
        return Rectangle {GetScreenWidth() - kPanelWidth + 22.0f,
                          GetScreenHeight() - 125.0f, kPanelWidth - 44.0f, 14.0f};
    }

    void UpdateTimeSlider() {
        if (mode != ViewMode::System) return;
        Rectangle bounds = TimeSliderBounds();
        Vector2 mouse = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, bounds))
            draggingTimeSlider = true;
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) draggingTimeSlider = false;
        if (draggingTimeSlider) {
            float fraction = std::clamp((mouse.x - bounds.x) / bounds.width, 0.0f, 1.0f);
            timeScale = fraction * 100.0f;
        }
    }

    void EnterSystem() {
        if (selectedStar == nullptr) return;
        mode = ViewMode::System;
        selectedPlanet = nullptr;
        hoveredPlanet = nullptr;
        ResetCamera(camera, mode, galaxyCameraYaw, galaxyCameraPitch);
        float systemRadius = CurrentSystem().radius;
        camera.position = Vector3 {0.0f, systemRadius * 0.72f, systemRadius * 1.15f};
        camera.target = Vector3 {0.0f, 0.0f, 0.0f};
    }

    Rectangle SeedInputBounds() const { return Rectangle {430.0f, 14.0f, 150.0f, 32.0f}; }
    Rectangle StarCountInputBounds() const { return Rectangle {642.0f, 14.0f, 86.0f, 32.0f}; }
    Rectangle ApplyInputBounds() const { return Rectangle {738.0f, 14.0f, 92.0f, 32.0f}; }

    std::size_t InputPositionAt(const Rectangle &bounds, const std::string &value, float mouseX) const {
        float localX = std::max(0.0f, mouseX - bounds.x - 9.0f);
        std::size_t position = 0;
        for (; position < value.size(); ++position) {
            float left = MeasureTextEx(viewerFont, value.substr(0, position).c_str(), 17.0f, 1.0f).x;
            float right = MeasureTextEx(viewerFont, value.substr(0, position + 1).c_str(), 17.0f, 1.0f).x;
            if (localX < (left + right) * 0.5f) break;
        }
        return position;
    }

    void DrawInputBox(Rectangle bounds, const std::string &value, const char *label, int inputIndex) {
        bool active = activeInput == inputIndex + 1;
        TextLine(label, static_cast<int>(bounds.x), static_cast<int>(bounds.y - 12.0f), 11,
                 Color {123, 157, 183, 255});
        DrawRectangleRounded(bounds, 0.16f, 6, Color {19, 31, 48, 235});
        DrawRectangleRoundedLinesEx(bounds, 0.16f, 6, 1.0f,
                                    active ? Color {88, 198, 255, 255} : Color {72, 104, 133, 255});
        std::size_t selectionStart = std::min(inputCursor[inputIndex], inputAnchor[inputIndex]);
        std::size_t selectionEnd = std::max(inputCursor[inputIndex], inputAnchor[inputIndex]);
        if (active && selectionStart != selectionEnd) {
            float start = MeasureTextEx(viewerFont, value.substr(0, selectionStart).c_str(), 17.0f, 1.0f).x;
            float end = MeasureTextEx(viewerFont, value.substr(0, selectionEnd).c_str(), 17.0f, 1.0f).x;
            DrawRectangle(static_cast<int>(bounds.x + 8.0f + start), static_cast<int>(bounds.y + 5.0f),
                          static_cast<int>(end - start), 22, Color {45, 125, 180, 220});
        }
        TextLine(value.c_str(), static_cast<int>(bounds.x + 9.0f), static_cast<int>(bounds.y + 7.0f), 17, WHITE);
        if (active) {
            float cursorX = MeasureTextEx(viewerFont, value.substr(0, inputCursor[inputIndex]).c_str(), 17.0f, 1.0f).x;
            DrawLine(static_cast<int>(bounds.x + 9.0f + cursorX), static_cast<int>(bounds.y + 6.0f),
                     static_cast<int>(bounds.x + 9.0f + cursorX), static_cast<int>(bounds.y + 27.0f), WHITE);
        }
    }

    bool ApplyGalaxyInputs() {
        char *end = nullptr;
        long newSeed = std::strtol(seedInput.c_str(), &end, 10);
        if (end == seedInput.c_str() || *end != '\0' || newSeed < 0 || newSeed > 99999999) return false;
        long newStarCount = std::strtol(starCountInput.c_str(), &end, 10);
        if (end == starCountInput.c_str() || *end != '\0' || newStarCount < 32 || newStarCount > 64) return false;
        seed = static_cast<int>(newSeed);
        starCount = static_cast<int>(newStarCount);
        bool rebuilt = RebuildGalaxy();
        if (rebuilt) activeInput = 0;
        return rebuilt;
    }

    void HandleGalaxySettingsInput() {
        Vector2 mouse = GetMousePosition();
        Rectangle bounds[] = {SeedInputBounds(), StarCountInputBounds(), ApplyInputBounds()};
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            activeInput = 0;
            draggingInput = false;
            for (int i = 0; i < 3; ++i) {
                if (CheckCollisionPointRec(mouse, bounds[i])) {
                    inputConsumed = true;
                    activeInput = i + 1;
                    if (i < 2) {
                        std::string &input = i == 0 ? seedInput : starCountInput;
                        inputCursor[i] = InputPositionAt(bounds[i], input, mouse.x);
                        inputAnchor[i] = inputCursor[i];
                        draggingInput = true;
                    } else {
                        ApplyGalaxyInputs();
                    }
                    break;
                }
            }
        }
        if (activeInput != 0) inputConsumed = true;
        if (draggingInput && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && activeInput <= 2) {
            int index = activeInput - 1;
            std::string &input = index == 0 ? seedInput : starCountInput;
            inputCursor[index] = InputPositionAt(bounds[index], input, mouse.x);
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) draggingInput = false;
        if (activeInput == 0) return;
        if (activeInput == 3) {
            if (IsKeyPressed(KEY_ENTER)) ApplyGalaxyInputs();
            return;
        }
        int index = activeInput - 1;
        std::string &input = index == 0 ? seedInput : starCountInput;
        std::size_t &cursor = inputCursor[index];
        std::size_t &anchor = inputAnchor[index];
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (IsKeyPressed(KEY_LEFT)) cursor = cursor > 0 ? cursor - 1 : 0;
        if (IsKeyPressed(KEY_RIGHT)) cursor = std::min(cursor + 1, input.size());
        if (IsKeyPressed(KEY_HOME)) cursor = 0;
        if (IsKeyPressed(KEY_END)) cursor = input.size();
        if (!shift) anchor = cursor;
        if (control && IsKeyPressed(KEY_A)) anchor = 0, cursor = input.size();
        std::size_t selectionStart = std::min(cursor, anchor);
        std::size_t selectionEnd = std::max(cursor, anchor);
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE)) {
            if (selectionStart != selectionEnd) {
                input.erase(selectionStart, selectionEnd - selectionStart);
                cursor = anchor = selectionStart;
            } else if (IsKeyPressed(KEY_BACKSPACE) && cursor > 0) {
                input.erase(cursor - 1, 1);
                --cursor;
                anchor = cursor;
            } else if (IsKeyPressed(KEY_DELETE) && cursor < input.size()) {
                input.erase(cursor, 1);
            }
        }
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= '0' && key <= '9') {
                std::size_t maxLength = index == 0 ? 8 : 2;
                selectionStart = std::min(cursor, anchor);
                selectionEnd = std::max(cursor, anchor);
                if (selectionStart != selectionEnd) {
                    input.erase(selectionStart, selectionEnd - selectionStart);
                    cursor = anchor = selectionStart;
                }
                if (input.size() < maxLength) {
                    input.insert(cursor, 1, static_cast<char>(key));
                    ++cursor;
                    anchor = cursor;
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_ENTER)) ApplyGalaxyInputs();
    }

    void LeaveSystem() {
        mode = ViewMode::Galaxy;
        selectedPlanet = nullptr;
        hoveredPlanet = nullptr;
        ResetCamera(camera, mode, galaxyCameraYaw, galaxyCameraPitch);
    }

    bool MouseOnPanel() const {
        return GetMousePosition().x >= static_cast<float>(GetScreenWidth()) - kPanelWidth;
    }

    void PickGalaxyObject() {
        hoveredStar = nullptr;
        if (MouseOnPanel() || IsCursorHidden()) return;
        Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
        Vector3 rayEnd = ray.position + ray.direction * 300.0f;
        float closest = 1.7f;
        for (const auto &star: stars) {
            Vector3 raySpan = rayEnd - ray.position;
            float squaredLength = Vector3LengthSqr(raySpan);
            float parameter = squaredLength < 0.000001f ? 0.0f :
                              Vector3DotProduct(star.position - ray.position, raySpan) / squaredLength;
            float distance = Vector3Distance(ray.position + raySpan * parameter, star.position);
            if (distance < closest) {
                closest = distance < star.galaxyScale * 0.25f ? 0.0f : distance;
                hoveredStar = &star;
            }
        }
    }

    void PickSystemObject() {
        hoveredPlanet = nullptr;
        if (MouseOnPanel() || IsCursorHidden()) return;
        Vector2 mouse = GetMousePosition();
        float closest = FLT_MAX;
        for (const auto &planet: CurrentSystem().planets) {
            Vector2 screen = GetWorldToScreen(planet.position, camera);
            float distance = Vector3Length(Vector3Subtract(planet.position, camera.position));
            float dx = screen.x - mouse.x;
            float dy = screen.y - mouse.y;
            float screenDistance = dx * dx + dy * dy;
            if (screenDistance <= 14.0f * 14.0f && distance < closest) {
                closest = distance;
                hoveredPlanet = &planet;
            }
        }
    }

    void HandleGalaxyInput() {
        if (activeInput != 0) return;
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_S)) && selectedStar != nullptr) {
            EnterSystem();
            return;
        }
        Rectangle enterButton {GetScreenWidth() - kPanelWidth + 22.0f,
                               GetScreenHeight() - 104.0f, kPanelWidth - 44.0f, 38.0f};
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), enterButton)) {
            EnterSystem();
            return;
        }
        if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || MouseOnPanel()) return;
        if (hoveredStar == nullptr) {
            selectedStar = nullptr;
            return;
        }
        double now = GetTime();
        bool doubleClick = previousClickedStar == hoveredStar && now - previousClickTime < 0.34;
        selectedStar = hoveredStar;
        previousClickedStar = hoveredStar;
        previousClickTime = now;
        if (doubleClick) EnterSystem();
    }

    void HandleSystemInput() {
        Rectangle backButton {22.0f, 20.0f, 112.0f, 38.0f};
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_G) ||
            (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), backButton))) {
            LeaveSystem();
            return;
        }
        if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || MouseOnPanel()) return;
        if (hoveredPlanet == nullptr) {
            selectedPlanet = nullptr;
            return;
        }
        double now = GetTime();
        bool doubleClick = previousClickedPlanet == hoveredPlanet && now - previousClickTime < 0.34;
        selectedPlanet = hoveredPlanet;
        previousClickedPlanet = hoveredPlanet;
        previousClickTime = now;
        if (doubleClick) {
            camera.target = hoveredPlanet->position;
        }
    }

    void Update() {
        if (shutdown) return;
        inputConsumed = false;
        double now = GetTime();
        if (lastFrameTime < 0.0) lastFrameTime = now;
        simulationTime += std::max(0.0, now - lastFrameTime) * timeScale;
        lastFrameTime = now;
#if defined(__EMSCRIPTEN__)
        bool needHide = false;
#else
        bool needHide = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
#endif
        if (IsCursorHidden() != needHide) {
            if (needHide) DisableCursor();
            else EnableCursor();
        }
        bool editingInput = mode == ViewMode::Galaxy && activeInput != 0;
        if (!editingInput) CameraControl(&camera, mode, galaxyCameraYaw, galaxyCameraPitch);
        if (mode == ViewMode::Galaxy) HandleGalaxySettingsInput();
        UpdateTimeSlider();
        UpdatePlanetPositions(now);
        if (!editingInput && !inputConsumed && IsKeyPressed(KEY_Z))
            ResetCamera(camera, mode, galaxyCameraYaw, galaxyCameraPitch);

        if (mode == ViewMode::Galaxy) {
            PickGalaxyObject();
            if (activeInput == 0 && !inputConsumed) HandleGalaxyInput();
        } else {
            PickSystemObject();
            HandleSystemInput();
        }
    }

    void DrawGalaxyScene(float time) {
        (void)time;
        DrawBackgroundStars(camera, false);
        float gridAlpha = std::clamp((Vector3Distance(camera.position, camera.target) - 22.0f) / 34.0f, 0.0f, 1.0f) * 0.3f;
        DrawCachedLines(galaxyGridLines, WithAlpha(Color {105, 153, 181, 255}, gridAlpha));
        DrawGalaxyLines(galaxyLines, stars, camera);
        for (const auto &star: stars) DrawGalaxyStarPoint(star, camera);
    }

    void DrawSystemScene(float time) {
        (void)time;
        const auto &system = CurrentSystem();
        DrawBackgroundStars(camera);
        DrawStarBlack(system.star, camera);
        DrawStarSpot(system.star, camera, StarVisualScale(system.star, camera));
        DrawStarMass(system.star, camera, time);
        if (selectedPlanet == nullptr) DrawCubeWiresV(system.star.position, system.star.selectionSize, WHITE);
        for (const auto &planet: system.planets) {
            Vector3 center {};
            if (planet.parentIndex >= 0)
                center = system.planets[static_cast<std::size_t>(planet.parentIndex)].position;
            DrawOrbit(center, planet);
            DrawPlanet(planet, &planet == selectedPlanet);
        }
    }

    void DrawTopBar() {
        DrawRectangle(0, 0, GetScreenWidth() - static_cast<int>(kPanelWidth), 62,
                      Color {5, 12, 23, 218});
        if (mode == ViewMode::Galaxy) {
            TextLine("GALAXY MAP", 24, 17, 22, Color {177, 232, 255, 255});
            TextLine(galaxySummary.c_str(), 24, 42, 14, Color {121, 154, 179, 255});
            DrawInputBox(SeedInputBounds(), seedInput, "seed 0-99999999", 0);
            DrawInputBox(StarCountInputBounds(), starCountInput, "stars 32-64", 1);
            DrawButton(ApplyInputBounds(), "APPLY", 46, true);
        } else {
            DrawButton(Rectangle {22.0f, 20.0f, 112.0f, 38.0f}, "BACK  [G]", kBackButtonTextWidth);
            TextLine(selectedStar->data->name.c_str(), 160, 17, 22, Color {177, 232, 255, 255});
            TextLine(CurrentSystem().summary.c_str(), 160, 42, 14, Color {121, 154, 179, 255});
        }
    }

    void DrawGalaxyOverlay() {
        int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
        if (selectedStar != nullptr) {
            DrawPanel(selectedStar->panel);
        } else {
            DrawPanelBackground();
            TextLine("SELECT A STAR", x, 94, 20, Color {137, 221, 255, 255});
            TextLine("Left click a star to inspect", x, 132, 16, Color {178, 197, 213, 255});
            TextLine("its generated parameters.", x, 155, 16, Color {178, 197, 213, 255});
            TextLine("Press Enter or double click", x, 203, 16, Color {178, 197, 213, 255});
            TextLine("to enter the planetary system.", x, 226, 16, Color {178, 197, 213, 255});
        }
        DrawButton(Rectangle {static_cast<float>(x), GetScreenHeight() - 104.0f, kPanelWidth - 44.0f, 38.0f},
                   "ENTER SYSTEM  [ENTER]", kEnterButtonTextWidth, selectedStar != nullptr);
        TextLine("MMB orbit  /  pitch follows game limits", x, GetScreenHeight() - 54, 14,
                 Color {98, 131, 159, 255});
        TextLine("Arrows rotate  /  Z reset", x, GetScreenHeight() - 33, 14, Color {98, 131, 159, 255});
    }

    void DrawSystemOverlay() {
        if (selectedPlanet != nullptr) DrawPanel(selectedPlanet->panel);
        else DrawPanel(selectedStar->panel);
        int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
        Rectangle slider = TimeSliderBounds();
        DrawRectangleRounded(slider, 0.5f, 4, Color {25, 43, 61, 255});
        float fraction = timeScale / 100.0f;
        DrawRectangleRounded(Rectangle {slider.x, slider.y, slider.width * fraction, slider.height},
                             0.5f, 4, Color {88, 198, 255, 255});
        DrawCircle(static_cast<int>(slider.x + slider.width * fraction),
                   static_cast<int>(slider.y + slider.height * 0.5f), 7.0f, WHITE);
        TextLine(FormatString("time speed  %.1fx", timeScale).c_str(), x, slider.y - 22, 14,
                 Color {123, 157, 183, 255});
        TextLine("Click another body to inspect it", x, GetScreenHeight() - 54, 14, Color {98, 131, 159, 255});
        TextLine("ESC / G returns to the galaxy map", x, GetScreenHeight() - 33, 14, Color {98, 131, 159, 255});
    }

    void DrawHoverLabel() {
        if (IsCursorHidden()) return;
        if (mode == ViewMode::Galaxy && hoveredStar != nullptr && hoveredStar != selectedStar) {
            Vector2 screen = GetWorldToScreen(hoveredStar->position, camera);
            DrawTextEx(viewerFont, hoveredStar->data->name.c_str(),
                       Vector2 {screen.x - hoveredStar->labelWidth * 0.5f, screen.y - 22.0f},
                       16.0f, 1.0f, LIGHTGRAY);
        } else if (mode == ViewMode::System && hoveredPlanet != nullptr && hoveredPlanet != selectedPlanet) {
            Vector2 screen = GetWorldToScreen(hoveredPlanet->position, camera);
            DrawTextEx(viewerFont, hoveredPlanet->name.c_str(),
                       Vector2 {screen.x - hoveredPlanet->labelWidth * 0.5f, screen.y - 22.0f},
                       16.0f, 1.0f, LIGHTGRAY);
        }
    }

    void Draw() {
        float time = static_cast<float>(GetTime());
        BeginDrawing();
        ClearBackground(Color {4, 8, 17, 255});
        if (mode == ViewMode::Galaxy && EnsureBloomTargets(GetScreenWidth(), GetScreenHeight())) {
            BeginMultisampleTextureMode(galaxySceneMultisampleTarget);
            Color linearBackground = LinearBackground(Color {5, 21, 26, 255});
            rlClearColor(linearBackground.r, linearBackground.g, linearBackground.b, 255);
            rlClearScreenBuffers();
            BeginMode3D(camera);
            DrawGalaxyScene(time);
            EndMode3D();
            ResolveMultisampleTexture(galaxySceneMultisampleTarget, galaxySceneTarget);

            BeginTextureMode(bloomPrefilterTarget);
            BeginOpaqueBlit();
            BeginShaderMode(bloomPrefilterShader);
            SetSourceTexelSize(bloomPrefilterShader, galaxySceneTarget.texture);
            const float threshold = GammaToLinearSpace(1.2f);
            const float knee = threshold * 0.3f + 1.0e-5f;
            SetShaderFloat(bloomPrefilterShader, "prefilterOffset", -0.5f);
            SetShaderFloat(bloomPrefilterShader, "threshold", threshold);
            SetShaderVector3(bloomPrefilterShader, "curve",
                             Vector3 {threshold - knee, knee * 2.0f, 0.25f / knee});
            DrawFlippedTexture(galaxySceneTarget.texture, bloomPrefilterTarget.texture.width,
                               bloomPrefilterTarget.texture.height);
            EndShaderMode();
            EndOpaqueBlit();
            EndTextureMode();

            Texture2D bloomSource = bloomPrefilterTarget.texture;
            for (int i = 0; i < bloomIterationCount; ++i) {
                auto &level = bloomDownLevels[static_cast<std::size_t>(i)];
                BeginTextureMode(level);
                BeginOpaqueBlit();
                Shader shader = i == 0 ? bloomDownsampleFirstShader : bloomDownsampleShader;
                BeginShaderMode(shader);
                SetSourceTexelSize(shader, bloomSource);
                DrawFlippedTexture(bloomSource, level.texture.width, level.texture.height);
                EndShaderMode();
                EndOpaqueBlit();
                EndTextureMode();
                bloomSource = level.texture;
            }

            for (int i = bloomIterationCount - 2; i >= 0; --i) {
                auto &level = bloomUpLevels[static_cast<std::size_t>(i)];
                BeginTextureMode(level);
                BeginOpaqueBlit();
                BeginShaderMode(bloomUpsampleShader);
                SetSourceTexelSize(bloomUpsampleShader, bloomSource, bloomSampleScale);
                int baseLocation = GetShaderLocation(bloomUpsampleShader, "baseTexture");
                if (baseLocation >= 0)
                    SetShaderValueTexture(bloomUpsampleShader, baseLocation,
                                          bloomDownLevels[static_cast<std::size_t>(i)].texture);
                DrawFlippedTexture(bloomSource, level.texture.width, level.texture.height);
                EndShaderMode();
                EndOpaqueBlit();
                EndTextureMode();
                bloomSource = level.texture;
            }

            BeginTextureMode(galaxyFxaaTarget);
            BeginOpaqueBlit();
            BeginShaderMode(bloomCompositeShader);
            int bloomLocation = GetShaderLocation(bloomCompositeShader, "bloomTexture");
            if (bloomLocation >= 0)
                SetShaderValueTexture(bloomCompositeShader, bloomLocation, bloomSource);
            int dirtLocation = GetShaderLocation(bloomCompositeShader, "lensDirtTexture");
            if (dirtLocation >= 0)
                SetShaderValueTexture(bloomCompositeShader, dirtLocation, bloomLensDirtTexture);
            int lutLocation = GetShaderLocation(bloomCompositeShader, "gradingLutTexture");
            if (lutLocation >= 0)
                SetShaderValueTexture(bloomCompositeShader, lutLocation, galaxyColorGradingLut);
            SetSourceTexelSize(bloomCompositeShader, bloomSource, bloomSampleScale);
            SetShaderFloat(bloomCompositeShader, "intensity", 0.3f);
            SetShaderFloat(bloomCompositeShader, "dirtIntensity", 3.0f);
            DrawFlippedTexture(galaxySceneTarget.texture, galaxyFxaaTarget.texture.width,
                               galaxyFxaaTarget.texture.height);
            EndShaderMode();
            EndOpaqueBlit();
            EndTextureMode();

            BeginOpaqueBlit();
            BeginShaderMode(galaxyFxaaShader);
            SetSourceTexelSize(galaxyFxaaShader, galaxyFxaaTarget.texture);
            SetShaderFloat(galaxyFxaaShader, "qualitySubpix", 0.75f);
            SetShaderFloat(galaxyFxaaShader, "qualityEdgeThreshold", 0.165999994f);
            SetShaderFloat(galaxyFxaaShader, "qualityEdgeThresholdMin", 0.0833000019f);
            DrawFlippedTexture(galaxyFxaaTarget.texture, GetScreenWidth(), GetScreenHeight());
            EndShaderMode();
            EndOpaqueBlit();
        } else {
            BeginMode3D(camera);
            if (mode == ViewMode::Galaxy) DrawGalaxyScene(time);
            else DrawSystemScene(time);
            EndMode3D();
        }
        DrawHoverLabel();
        DrawTopBar();
        if (mode == ViewMode::Galaxy) DrawGalaxyOverlay();
        else DrawSystemOverlay();
        EndDrawing();
    }

    void Shutdown() {
        if (shutdown) return;
        shutdown = true;
        if (galaxy != nullptr) {
            galaxy->release();
            galaxy = nullptr;
        }
        if (generatorThreadsInitialized) {
            dspugen::Planet::releaseThread();
            dspugen::Star::releaseThread();
            dspugen::Galaxy::releaseThread();
            generatorThreadsInitialized = false;
        }
        if (hasCustomFont) {
            UnloadFont(viewerFont);
            hasCustomFont = false;
        }
        if (starMassModel.meshCount != 0) {
            starMassModel.materials[0].shader.id = rlGetShaderIdDefault();
            starMassModel.materials[0].shader.locs = rlGetShaderLocsDefault();
            UnloadModel(starMassModel);
        }
        if (starBlackModel.meshCount != 0) {
            starBlackModel.materials[0].shader.id = rlGetShaderIdDefault();
            starBlackModel.materials[0].shader.locs = rlGetShaderLocsDefault();
            UnloadModel(starBlackModel);
        }
        if (starSpotModel.meshCount != 0) {
            starSpotModel.materials[0].shader.id = rlGetShaderIdDefault();
            starSpotModel.materials[0].shader.locs = rlGetShaderLocsDefault();
            UnloadModel(starSpotModel);
        }
        if (galaxyStarPointModel.meshCount != 0) {
            galaxyStarPointModel.materials[0].shader.id = rlGetShaderIdDefault();
            galaxyStarPointModel.materials[0].shader.locs = rlGetShaderLocsDefault();
            galaxyStarPointModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = {};
            UnloadModel(galaxyStarPointModel);
        }
        if (IsTextureValid(galaxyStarPointTexture)) UnloadTexture(galaxyStarPointTexture);
        if (IsTextureValid(galaxyLineTexture)) UnloadTexture(galaxyLineTexture);
        if (IsTextureValid(galaxyColorGradingLut)) UnloadTexture(galaxyColorGradingLut);
        if (IsTextureValid(bloomLensDirtTexture)) UnloadTexture(bloomLensDirtTexture);
        galaxyColorGradingLut = {};
        bloomLensDirtTexture = {};
        UnloadBloomTargets();
        if (hasGalaxyFxaaShader) UnloadShader(galaxyFxaaShader);
        if (hasBloomShaders) {
            UnloadShader(bloomCompositeShader);
            UnloadShader(bloomUpsampleShader);
            UnloadShader(bloomDownsampleShader);
            UnloadShader(bloomDownsampleFirstShader);
            UnloadShader(bloomPrefilterShader);
        }
        if (hasGalaxyLineShader) UnloadShader(galaxyLineShader);
        if (hasGalaxyStarPointShader) UnloadShader(galaxyStarPointShader);
        if (hasStarBlackShader) UnloadShader(starBlackShader);
        if (hasStarSpotShader) UnloadShader(starSpotShader);
        if (hasStarSurfaceShader) UnloadShader(starSurfaceShader);
        CloseWindow();
    }
};

Viewer *Viewer::current = nullptr;

} // namespace

int main(int argc, char *argv[]) {
    int seed = argc > 1 ? std::atoi(argv[1]) : 0;
    int starCount = argc > 2 ? std::atoi(argv[2]) : 64;
    starCount = std::clamp(starCount, 1, 256);
    Viewer viewer(seed, starCount, argc > 3 ? argv[3] : DSP_VIEWER_DEFAULT_FONT);
    if (!viewer.Init()) return 1;
    viewer.Run();
    return 0;
}
