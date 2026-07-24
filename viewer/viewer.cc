/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <algorithm>
#include <cmath>
#include <cfloat>
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

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
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
constexpr float kSystemOrbitScale = 10.0f;
constexpr int kOrbitSegments = 72;
constexpr int kEnterButtonTextWidth = 243;
constexpr int kBackButtonTextWidth = 80;
constexpr int kViewerFontSize = 64;

Font viewerFont{};
bool hasCustomFont = false;

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
    std::string footerKey;
    std::string footerValue;
};

struct StarViewData {
    const dspugen::Star *data = nullptr;
    Vector3 position{};
    Color color{};
    float radius = 1.0f;
    float flickerSpeed = 1.0f;
    float flickerPhase = 0.0f;
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

void GradientColor(float value, Color &color) {
    const struct {
        float position;
        Vector3 color;
    } stops[] = {
        {0.0f, {1.0f, 0.229f, 0.1745f}},
        {0.07f, {1.0f, 0.4373f, 0.1765f}},
        {0.32f, {1.0f, 0.9515f, 0.1557f}},
        {0.5f, {0.9879f, 1.0f, 0.6179f}},
        {0.7235f, {0.9764f, 1.0f, 0.978f}},
        {0.82f, {0.58f, 0.8963f, 1.0f}},
        {0.92f, {0.1176f, 0.6548f, 1.0f}},
        {1.0f, {0.0991f, 0.34f, 1.0f}},
    };

    color.a = 255;
    if (value == 0.0f) {
        color.r = static_cast<unsigned char>(std::roundf(stops[0].color.x * 255.0f));
        color.g = static_cast<unsigned char>(std::roundf(stops[0].color.y * 255.0f));
        color.b = static_cast<unsigned char>(std::roundf(stops[0].color.z * 255.0f));
        return;
    }
    for (std::size_t i = 1; i < std::size(stops); ++i) {
        if (value > stops[i].position) continue;
        const auto &left = stops[i - 1];
        const auto &right = stops[i];
        float t = (value - left.position) / (right.position - left.position);
        color.r = static_cast<unsigned char>(std::roundf((left.color.x + (right.color.x - left.color.x) * t) * 255.0f));
        color.g = static_cast<unsigned char>(std::roundf((left.color.y + (right.color.y - left.color.y) * t) * 255.0f));
        color.b = static_cast<unsigned char>(std::roundf((left.color.z + (right.color.z - left.color.z) * t) * 255.0f));
        return;
    }
}

Color StarColor(const dspugen::Star *star) {
    switch (star->type) {
        case dspugen::EStarType::WhiteDwarf:
            return Color {118, 118, 118, 255};
        case dspugen::EStarType::NeutronStar:
            return Color {128, 97, 255, 255};
        case dspugen::EStarType::BlackHole:
            return Color {0, 0, 0, 255};
        default: {
            Color color{};
            GradientColor(star->color, color);
            return color;
        }
    }
}

float StarRadius(const dspugen::Star *star) {
    switch (star->type) {
        case dspugen::EStarType::GiantStar:
            return 4.0f * 0.2f;
        case dspugen::EStarType::WhiteDwarf:
        case dspugen::EStarType::NeutronStar:
            return 0.8f * 0.2f;
        case dspugen::EStarType::BlackHole:
            return 1.0f * 0.2f;
        default:
            return 1.5f * 0.2f;
    }
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

PanelData CreatePlanetPanel(const dspugen::Planet *planet, int galaxySeed, const std::string &name) {
    const auto *theme = dspugen::themeProtoSet.select(planet->theme);
    int veinSpots = 0;
    for (int i = 1; i < 15; ++i) veinSpots += planet->veinSpot[i];
    PanelData panel;
    panel.title = "PLANET PARAMETERS";
    panel.name = name;
    panel.subtitle = PlanetTypeName(planet->type);
    panel.rows = {
        {"galaxy seed", std::to_string(galaxySeed)},
        {"planet ID", std::to_string(planet->id)},
        {"generation seed", std::to_string(planet->seed)},
        {"star", planet->star->name},
        {"orbit number", std::to_string(planet->number)},
        {"orbit parent", planet->orbitAround == 0 ? "star" : std::to_string(planet->orbitAround)},
        {"orbit index", std::to_string(planet->orbitIndex)},
        {"orbit radius", FormatString("%.4f AU", planet->orbitRadius)},
        {"sun distance", FormatString("%.4f AU", planet->sunDistance)},
        {"real radius", FormatString("%.1f km", planet->realRadius())},
        {"luminosity", FormatString("%.2f", planet->luminosity)},
        {"temperature bias", FormatString("%+.3f", planet->temperatureBias)},
        {"habitable bias", FormatString("%.3f", planet->habitableBias)},
        {"theme", theme == nullptr ? "unknown" : theme->name},
        {"theme ID", std::to_string(planet->theme)},
        {"algorithm", std::to_string(planet->algoId)},
        {"gas entries", std::to_string(planet->gasItems.size())},
        {"vein spots", std::to_string(veinSpots)},
    };
    panel.footerKey = "singularities";
    panel.footerValue = SingularityName(planet->singularity);
    return panel;
}

void ResetCamera(Camera3D &camera, ViewMode mode) {
    camera.up = mode == ViewMode::Galaxy ? Vector3 {0.0f, -1.0f, 0.0f} : Vector3 {0.0f, 1.0f, 0.0f};
    camera.fovy = mode == ViewMode::Galaxy ? 45.0f : 52.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    if (mode == ViewMode::Galaxy) {
        camera.position = Vector3 {40.0f, -40.0f, 40.0f};
        camera.target = Vector3 {0.0f, -10.0f, 0.0f};
    } else {
        camera.position = Vector3 {0.0f, 24.0f, 30.0f};
        camera.target = Vector3 {0.0f, 0.0f, 0.0f};
    }
}

void CameraControl(Camera *camera) {
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

void DrawFlicker(Vector3 position, float radius, float pulse) {
    BeginBlendMode(BLEND_ADDITIVE);
    DrawSphere(position, radius, WithAlpha(WHITE, 0.015f + pulse * 0.055f));
    EndBlendMode();
}

void DrawStar(const StarViewData &star, float time, bool selected) {
    float pulse = 0.5f + 0.5f * std::sin(time * star.flickerSpeed + star.flickerPhase);
    DrawSphere(star.position, star.radius, star.color);
    DrawFlicker(star.position, star.radius, pulse);
    if (selected) {
        DrawCubeWiresV(star.position, star.selectionSize, WHITE);
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

void DrawPlanet(const PlanetViewData &planet, float time, bool selected) {
    float pulse = 0.5f + 0.5f * std::sin(time * 1.7f + planet.flickerPhase);
    DrawSphere(planet.position, planet.radius, planet.color);
    DrawFlicker(planet.position, planet.radius, pulse);
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
    TextLine(row.key, x, y, 14, Color {123, 157, 183, 255});
    TextLine(row.value.c_str(), x + 112, y, 14, RAYWHITE);
    return y + 24;
}

void DrawPanel(const PanelData &panel) {
    DrawPanelBackground();
    int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
    TextLine(panel.title.c_str(), x, 25, 20, Color {137, 221, 255, 255});
    TextLine(panel.name.c_str(), x, 58, 24, WHITE);
    TextLine(panel.subtitle.c_str(), x, 89, 14, Color {161, 187, 208, 255});
    int y = 126;
    for (const auto &row: panel.rows) y = DrawKeyValue(row, y);
    if (!panel.footerKey.empty()) TextLine(panel.footerKey.c_str(), x, y + 8, 14, Color {123, 157, 183, 255});
    if (!panel.footerValue.empty()) TextLine(panel.footerValue.c_str(), x, y + (panel.footerKey.empty() ? 16 : 31), 13,
                                             panel.footerKey.empty() ? Color {98, 131, 159, 255} : RAYWHITE);
}

class Viewer {
public:
    Viewer(int seed, int starCount, std::string fontPath)
        : seed(seed), starCount(starCount), fontPath(std::move(fontPath)) {}

    bool Init() {
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(kScreenWidth, kScreenHeight, "DSPSeedCalc Universe Viewer");
        viewerFont = GetFontDefault();
        if (!fontPath.empty() && FileExists(fontPath.c_str())) {
            Font loadedFont = LoadFontEx(fontPath.c_str(), kViewerFontSize, nullptr, 0);
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
        dspugen::loadProtoSets();
        dspugen::Galaxy::initThread();
        dspugen::Star::initThread();
        dspugen::Planet::initThread();
        galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, seed, starCount);
        if (galaxy == nullptr) return false;
        galaxy->ensureAllGas();

        stars.reserve(galaxy->stars.size());
        systems.resize(galaxy->stars.size());
        for (const auto *star: galaxy->stars) {
            Vector3 position {
                static_cast<float>(star->position.x),
                static_cast<float>(star->position.y - 10.0f),
                static_cast<float>(star->position.z),
            };
            float radius = StarRadius(star);
            StarViewData view;
            view.data = star;
            view.position = position;
            view.color = StarColor(star);
            view.radius = radius;
            view.flickerSpeed = 1.3f + Hash01(star->seed) * 2.0f;
            view.flickerPhase = star->seed * 0.01f;
            view.selectionSize = Vector3 {radius * 1.8f, radius * 1.8f, radius * 1.8f};
            view.labelWidth = MeasureTextEx(viewerFont, star->name.c_str(), 16.0f, 1.0f).x;
            view.panel = CreateStarPanel(star, seed);
            stars.emplace_back(std::move(view));
        }
        for (std::size_t i = 0; i < stars.size(); ++i) BuildSystemCache(i);
        galaxySummary = FormatString("seed %d  /  %d stars  /  birth star #%d", seed, galaxy->starCount, galaxy->birthStarId);
        galaxyGridLines.reserve(36);
        constexpr int gridSlices = 8;
        constexpr float gridSpacing = 10.0f;
        float halfGrid = gridSlices * 0.5f * gridSpacing;
        for (int i = -gridSlices / 2; i <= gridSlices / 2; ++i) {
            float offset = static_cast<float>(i) * gridSpacing;
            galaxyGridLines.push_back(Line3DCache {{offset, 0.0f, -halfGrid}, {offset, 0.0f, halfGrid}});
            galaxyGridLines.push_back(Line3DCache {{-halfGrid, 0.0f, offset}, {halfGrid, 0.0f, offset}});
        }
        ResetCamera(camera, mode);
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
    double simulationTime = 0.0;
    double lastFrameTime = -1.0;
    float timeScale = 10.0f;
    bool draggingTimeSlider = false;

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
            view.panel = CreatePlanetPanel(planet, seed, view.name);
            system.radius = std::max(system.radius, Vector3Length(position) + radius);
            system.planets.emplace_back(std::move(view));
        }
        system.star = starView;
        system.star.position = Vector3 {};
        system.star.radius = starView.data->radius * 800.0f * 0.00025f;
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
        ResetCamera(camera, mode);
        float systemRadius = CurrentSystem().radius;
        camera.position = Vector3 {0.0f, systemRadius * 0.72f, systemRadius * 1.15f};
        camera.target = Vector3 {0.0f, 0.0f, 0.0f};
    }

    void LeaveSystem() {
        mode = ViewMode::Galaxy;
        selectedPlanet = nullptr;
        hoveredPlanet = nullptr;
        ResetCamera(camera, mode);
    }

    bool MouseOnPanel() const {
        return GetMousePosition().x >= static_cast<float>(GetScreenWidth()) - kPanelWidth;
    }

    void PickGalaxyObject() {
        hoveredStar = nullptr;
        if (MouseOnPanel() || IsCursorHidden()) return;
        Vector2 mouse = GetMousePosition();
        float closest = FLT_MAX;
        for (const auto &star: stars) {
            Vector2 screen = GetWorldToScreen(star.position, camera);
            Vector3 cameraSpace = Vector3Subtract(star.position, camera.position);
            float distance = Vector3Length(cameraSpace);
            float dx = screen.x - mouse.x;
            float dy = screen.y - mouse.y;
            float screenDistance = dx * dx + dy * dy;
            if (screenDistance <= 12.0f * 12.0f && distance < closest) {
                closest = distance;
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
        CameraControl(&camera);
        UpdateTimeSlider();
        UpdatePlanetPositions(now);
        if (IsKeyPressed(KEY_Z)) ResetCamera(camera, mode);

        if (mode == ViewMode::Galaxy) {
            PickGalaxyObject();
            HandleGalaxyInput();
        } else {
            PickSystemObject();
            HandleSystemInput();
        }
    }

    void DrawGalaxyScene(float time) {
        DrawCachedLines(galaxyGridLines, LIGHTGRAY);
        for (const auto &star: stars) {
            DrawStar(star, time, &star == selectedStar);
        }
    }

    void DrawSystemScene(float time) {
        const auto &system = CurrentSystem();
        DrawStar(system.star, time, selectedPlanet == nullptr);
        for (const auto &planet: system.planets) {
            Vector3 center {};
            if (planet.parentIndex >= 0)
                center = system.planets[static_cast<std::size_t>(planet.parentIndex)].position;
            DrawOrbit(center, planet);
            DrawPlanet(planet, time, &planet == selectedPlanet);
        }
    }

    void DrawTopBar() {
        DrawRectangle(0, 0, GetScreenWidth() - static_cast<int>(kPanelWidth), 62,
                      Color {5, 12, 23, 218});
        if (mode == ViewMode::Galaxy) {
            TextLine("GALAXY MAP", 24, 17, 22, Color {177, 232, 255, 255});
            TextLine(galaxySummary.c_str(), 24, 42, 12, Color {121, 154, 179, 255});
        } else {
            DrawButton(Rectangle {22.0f, 20.0f, 112.0f, 38.0f}, "BACK  [G]", kBackButtonTextWidth);
            TextLine(selectedStar->data->name.c_str(), 160, 17, 22, Color {177, 232, 255, 255});
            TextLine(CurrentSystem().summary.c_str(), 160, 42, 12, Color {121, 154, 179, 255});
        }
    }

    void DrawGalaxyOverlay() {
        int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
        if (selectedStar != nullptr) {
            DrawPanel(selectedStar->panel);
        } else {
            DrawPanelBackground();
            TextLine("SELECT A STAR", x, 94, 20, Color {137, 221, 255, 255});
            TextLine("Left click a star to inspect", x, 132, 15, Color {178, 197, 213, 255});
            TextLine("its generated parameters.", x, 155, 15, Color {178, 197, 213, 255});
            TextLine("Press Enter or double click", x, 203, 15, Color {178, 197, 213, 255});
            TextLine("to enter the planetary system.", x, 226, 15, Color {178, 197, 213, 255});
        }
        DrawButton(Rectangle {static_cast<float>(x), GetScreenHeight() - 104.0f, kPanelWidth - 44.0f, 38.0f},
                   "ENTER SYSTEM  [ENTER]", kEnterButtonTextWidth, selectedStar != nullptr);
        TextLine("RMB orbit  /  MMB pan  /  wheel zoom", x, GetScreenHeight() - 54, 12, Color {98, 131, 159, 255});
        TextLine("WASD move  /  arrows rotate  /  Z reset", x, GetScreenHeight() - 33, 12, Color {98, 131, 159, 255});
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
        TextLine(FormatString("time speed  %.1fx", timeScale).c_str(), x, slider.y - 22, 12,
                 Color {123, 157, 183, 255});
        TextLine("Click another body to inspect it", x, GetScreenHeight() - 54, 12, Color {98, 131, 159, 255});
        TextLine("ESC / G returns to the galaxy map", x, GetScreenHeight() - 33, 12, Color {98, 131, 159, 255});
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
        BeginMode3D(camera);
        if (mode == ViewMode::Galaxy) DrawGalaxyScene(time);
        else DrawSystemScene(time);
        EndMode3D();
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
        dspugen::Planet::releaseThread();
        dspugen::Star::releaseThread();
        dspugen::Galaxy::releaseThread();
        if (hasCustomFont) {
            UnloadFont(viewerFont);
            hasCustomFont = false;
        }
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
