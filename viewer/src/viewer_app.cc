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
#include <limits>
#include <cstdint>
#include <initializer_list>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#include "galaxy.hh"
#include "protoset.hh"
#include "settings.hh"
#include "util/dotnet35random.hh"
#include "camera_controller.hh"
#include "app_resources.hh"
#include "material_catalog.hh"
#include "panel_scroll.hh"
#include "post_process.hh"
#include "render_resources.hh"
#include "scene_renderer.hh"
#include "view_model.hh"
#include "view_model_builder.hh"
#include "viewer_selection.hh"
#include "viewer_pointer.hh"
#include "viewer_ui.hh"

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

namespace viewer {

constexpr int kScreenWidth = 1920;
constexpr int kScreenHeight = 1080;
constexpr int kGalaxyPanelFooterReserved = 84;
constexpr int kSystemPanelFooterReserved = 112;
constexpr float kSystemOrbitScale = 10.0f;
// `UIStarmapPlanet._OnCreate` writes 60 positions at six degree steps and leaves `loop` on.
constexpr int kOrbitSegments = 60;
// Star positions reach the starmap as `uPosition * 0.00025` and `StarGen.cs:197` sets
// `uPosition = position * 2400000`, so one generator unit is 600 starmap units.
constexpr float kStarmapPositionScale = 600.0f;
constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();
constexpr int kInputTextSize = 16;
constexpr float kDragThreshold = 5.0f;
constexpr double kDoubleClickSeconds = 0.34;
constexpr float kTimeScaleMin = 0.1f;
constexpr float kTimeScaleMax = 100.0f;
constexpr double kToastSeconds = 2.6;

#if defined(__EMSCRIPTEN__)
extern "C" int viewerBeginNumberEdit(float x, float y);
extern "C" void viewerFinishNumberEdit(int submit);

int WebViewportWidth() {
    return std::max(1, EM_ASM_INT({
        return Math.max(1, document.documentElement.clientWidth || window.innerWidth || 1);
    }));
}

int WebViewportHeight() {
    return std::max(1, EM_ASM_INT({
        return Math.max(1, document.documentElement.clientHeight || window.innerHeight || 1);
    }));
}
#endif

class Viewer {
public:
    Viewer(int seed, int starCount, std::string fontPath)
        : seed(seed), starCount(starCount), fontPath(std::move(fontPath)),
          seedInput(std::to_string(seed)), starCountInput(std::to_string(starCount)),
          sceneRenderer(resources, postProcess) {}

    void SetPostEffects(bool fxaa, int bloomLevel) { postProcess.SetPostEffects(fxaa, bloomLevel); }
    void SetOrbitWidth(float factor) { sceneRenderer.SetOrbitWidthFactor(factor); }

    bool Init() {
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#if defined(__EMSCRIPTEN__)
        InitWindow(WebViewportWidth(), WebViewportHeight(), "DSPSeedCalc Universe Viewer");
#else
        InitWindow(kScreenWidth, kScreenHeight, "DSPSeedCalc Universe Viewer");
#endif
        screen = CalculateViewerLayout(GetScreenWidth(), GetScreenHeight());
        if (!LoadRenderResources(resources, fontPath)) {
            Shutdown();
            return false;
        }
        SetTargetFPS(60);
        /* Esc navigates back inside the viewer; it must not close the window. */
        SetExitKey(KEY_NULL);
        cameraController.InitPlatformInput();

        dspugen::settings.hasPlanets = true;
        dspugen::settings.birthOnly = false;
        dspugen::settings.genName = true;
        dspugen::settings.noPosition = false;
        dspugen::loadProtoSets(ViewerRuntimePath("Prototypes"));
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
        selectedStar = kInvalidIndex;
        hoveredStar = kInvalidIndex;
        selectedPlanet = kInvalidIndex;
        hoveredPlanet = kInvalidIndex;
        focusedPlanet = kInvalidIndex;
        galaxyListEntries.clear();
        panelScrollOffset = 0.0f;
        listScrollOffset = 0.0f;
        previousClickedStar = kInvalidIndex;
        previousClickedPlanet = kInvalidIndex;
        previousClickTime = -1.0;
        mode = ViewMode::Galaxy;
        page = CompactPage::Map;
        simulationTime = 0.0;
        lastFrameTime = GetTime();

        galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, seed, starCount);
        if (galaxy == nullptr) return false;
        galaxy->ensureAllPlanets();
        galaxy->ensureAllGas();
        const auto veinAmounts = galaxy->ensureAllVeinAmounts();
        if (!veinAmounts.ok()) {
            std::fprintf(stderr, "viewer: exact mineral calculation failed for planet %d (algorithm %d): %s\n",
                         veinAmounts.planetId, veinAmounts.algoId,
                         dspugen::veinAmountResultName(veinAmounts.result));
        }

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
            view.labelWidth = MeasureTextEx(resources.font, star->name.c_str(), 16.0f, 1.0f).x;
            view.panel = CreateStarPanel(star, seed);
            view.massMaterial = StarMassMaterial(star);
            view.spotMaterial = StarSpotMaterial(star);
            stars.emplace_back(std::move(view));
            galaxyListEntries.push_back(CreateStarListEntry(star));
        }
        selectedStar = DefaultSelectedStarIndex(stars.size());
        galaxyLines = CreateGalaxyLines(galaxy, stars);
        for (std::size_t i = 0; i < stars.size(); ++i) BuildSystemCache(i);
        char summary[96];
        std::snprintf(summary, sizeof(summary), "Birth star   %s", stars.empty() ? "-" : stars[0].data->name.c_str());
        galaxySummary = summary;
        seedInput = std::to_string(seed);
        starCountInput = std::to_string(starCount);
        char title[128];
        std::snprintf(title, sizeof(title), "DSPSeedCalc Universe Viewer - seed %d / %d stars", seed, starCount);
        SetWindowTitle(title);
        cameraController.ResetGalaxy(camera);
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
#if defined(__EMSCRIPTEN__)
    friend int viewerBeginNumberEdit(float, float);
    friend void viewerFinishNumberEdit(int);
#endif

    RenderResources resources;
    PostProcess postProcess;
    int seed = 0;
    int starCount = 64;
    std::string fontPath;
    dspugen::Galaxy *galaxy = nullptr;
    Camera3D camera{};
    CameraController cameraController;
    SceneRenderer sceneRenderer;
    ViewMode mode = ViewMode::Galaxy;
    ViewerLayout screen{};
    ViewerPointer pointer;
    CompactPage page = CompactPage::Map;
    std::vector<Rectangle> labelBounds;
    std::vector<StarViewData> stars;
    std::vector<SystemViewCache> systems;
    std::vector<GalaxyLineData> galaxyLines;
    std::string galaxySummary;
    std::size_t selectedStar = kInvalidIndex;
    std::size_t hoveredStar = kInvalidIndex;
    std::size_t selectedPlanet = kInvalidIndex;
    std::size_t hoveredPlanet = kInvalidIndex;
    double previousClickTime = -1.0;
    std::size_t previousClickedStar = kInvalidIndex;
    std::size_t previousClickedPlanet = kInvalidIndex;
    bool shutdown = false;
    bool generatorThreadsInitialized = false;
    double simulationTime = 0.0;
    double lastFrameTime = -1.0;
    float timeScale = 1.0f;
    bool timePaused = false;
    Vector3 systemCameraTarget{};
    std::size_t focusedPlanet = kInvalidIndex;
    bool draggingTimeSlider = false;
    std::string seedInput;
    std::string starCountInput;
    int activeInput = 0;
    int invalidInput = 0;
    std::size_t inputCursor[2] = {};
    std::size_t inputAnchor[2] = {};
    bool draggingInput = false;
    bool inputConsumed = false;
    float panelScrollOffset = 0.0f;
    std::vector<BodyListEntry> galaxyListEntries;
    float listScrollOffset = 0.0f;
    bool showList = true;
    bool showHelp = false;
    std::size_t hoveredListEntry = kInvalidIndex;
    Vector2 pressPosition{};
    enum class PressArea { None, Map, List, Panel, Control };
    PressArea pressArea = PressArea::None;
    float pressScrollOffset = 0.0f;
    std::size_t pressedListEntry = kInvalidIndex;
    bool movedSincePress = false;
    bool multiTouch = false;
    bool pinchOnMap = false;
    float pinchSpan = 0.0f;
    bool primaryDragging = false;
    bool pendingRebuild = false;
    bool busyFrameDrawn = false;
    std::string toastText;
    Color toastColor = theme::kAccentSoft;
    double toastTime = -100.0;
    int currentCursor = MOUSE_CURSOR_DEFAULT;

    static void Loop() {
        if (current == nullptr) return;
        current->Update();
        current->Draw();
    }

    static void DrawSceneCallback(const void *context, float time) {
        auto &viewer = *const_cast<Viewer *>(static_cast<const Viewer *>(context));
        if (viewer.mode == ViewMode::Galaxy)
            viewer.sceneRenderer.DrawGalaxy(viewer.galaxyLines, viewer.stars, viewer.camera, time,
                                            viewer.selectedStar, kInvalidIndex);
        else
            viewer.sceneRenderer.DrawSystem(viewer.CurrentSystem(), viewer.camera, time);
    }

    SystemViewCache &CurrentSystem() {
        return systems[selectedStar];
    }

    const SystemViewCache &CurrentSystem() const {
        return systems[selectedStar];
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
            float radius = planet->realRadius() * 0.00025f;
            PlanetViewData view;
            view.data = planet;
            view.position = position;
            view.radius = radius;
            view.color = PlanetColor(planet->type);
            view.flickerPhase = planet->seed * 0.02f;
            view.parentIndex = parentIndex;
            view.orbitRadius = orbitRadius;
            view.orbitInclination = inclination;
            view.orbitLongitude = longitude;
            view.orbitPhase = angle;
            view.orbitAngle = angle;
            // `orbitRenderer.widthMultiplier = (orbitAroundPlanet == null) ? 0.7f * star.orbitScaler : 0.1f`
            view.orbitWidth = planet->orbitAroundPlanet == nullptr
                                  ? 0.7f * starView.data->orbitScaler
                                  : 0.1f;
            view.orbitStencilRef = planet->index + 101;
            view.orbitalPeriod = std::max(orbit.period, 1.0);
            view.orbitRotation = orbitRotation;
            view.orbitPoints.reserve(kOrbitSegments);
            for (int i = 0; i < kOrbitSegments; ++i) {
                float orbitAngle = static_cast<float>(i) * (PI / 180.0f) * 6.0f;
                view.orbitPoints.push_back(OrbitPoint(Vector3 {}, orbitRadius, orbitAngle, orbitRotation));
            }
            view.name = PlanetName(planet);
            view.labelWidth = MeasureTextEx(resources.font, view.name.c_str(), 16.0f, 1.0f).x;
            view.panel = CreatePlanetPanel(planet, view.name);
            view.thumbMaterial = PlanetThumbMaterial(planet);
            system.radius = std::max(system.radius, Vector3Length(position) + radius);
            system.listEntries.push_back(CreatePlanetListEntry(planet, view.name));
            system.planets.emplace_back(std::move(view));
        }
        system.star = starView;
        system.star.position = Vector3 {};
        system.star.radius = sceneRenderer.StarViewRadius(starView.data);
        // The whole cluster stays in the scene, so build the light copies the star draw calls need.
        // The panel and label are deliberately left empty: only the current star has a panel.
        system.otherStars.reserve(stars.size() - 1);
        for (std::size_t other = 0; other < stars.size(); ++other) {
            if (other == starIndex) continue;
            StarViewData view;
            view.data = stars[other].data;
            view.position = (stars[other].position - starView.position) * kStarmapPositionScale;
            view.galaxyColor = stars[other].galaxyColor;
            view.galaxyScale = stars[other].galaxyScale;
            view.radius = sceneRenderer.StarViewRadius(stars[other].data);
            view.massMaterial = stars[other].massMaterial;
            view.spotMaterial = stars[other].spotMaterial;
            system.otherStars.push_back(view);
        }
        char summary[96];
        std::snprintf(summary, sizeof(summary), "%s  /  %d planets", system.star.panel.subtitle.c_str(),
                      static_cast<int>(system.planets.size()));
        system.summary = summary;
    }

    void UpdatePlanetPositions() {
        if (mode != ViewMode::System || selectedStar == kInvalidIndex) return;
        auto &system = CurrentSystem();
        for (auto &planet: system.planets) {
            Vector3 center {};
            if (planet.parentIndex >= 0)
                center = system.planets[static_cast<std::size_t>(planet.parentIndex)].position;
            float angle = planet.orbitPhase +
                          static_cast<float>(simulationTime * 60.0 / planet.orbitalPeriod * 2.0 * PI);
            double phase = static_cast<double>(angle) / (2.0 * PI);
            phase -= std::floor(phase + 0.1);
            planet.orbitAngle = static_cast<float>(phase * 2.0 * PI);
            planet.position = OrbitPoint(center, planet.orbitRadius, planet.orbitAngle,
                                         planet.orbitRotation);
        }
    }

    static float TimeScaleFraction(float scale) {
        return std::log(scale / kTimeScaleMin) / std::log(kTimeScaleMax / kTimeScaleMin);
    }

    static float TimeScaleFromFraction(float fraction) {
        return kTimeScaleMin * std::pow(kTimeScaleMax / kTimeScaleMin, fraction);
    }

    Rectangle TimeSliderBounds() const {
        return {screen.panel.x + kPanelPadding, screen.panel.y + screen.panel.height - 26,
                screen.panel.width - 2 * kPanelPadding, 8};
    }

    Rectangle TimeSliderHitBounds() const {
        Rectangle bounds = TimeSliderBounds();
        return Rectangle {bounds.x - 8.0f, bounds.y - 12.0f, bounds.width + 16.0f, bounds.height + 24.0f};
    }

    void UpdateTimeSlider() {
        if (mode != ViewMode::System || !PanelVisible()) {
            draggingTimeSlider = false;
            return;
        }
        Rectangle bounds = TimeSliderBounds();
        Vector2 mouse = pointer.position;
        if (pointer.pressed && CheckCollisionPointRec(mouse, TimeSliderHitBounds()))
            draggingTimeSlider = true;
        if (!pointer.down) draggingTimeSlider = false;
        if (draggingTimeSlider) {
            float fraction = std::clamp((mouse.x - bounds.x) / bounds.width, 0.0f, 1.0f);
            timeScale = TimeScaleFromFraction(fraction);
            timePaused = false;
        }
    }

    void SetTimeScale(float scale) {
        timeScale = std::clamp(scale, kTimeScaleMin, kTimeScaleMax);
        timePaused = false;
    }

    void ShowToast(std::string text, Color color = theme::kAccentSoft) {
        toastText = std::move(text);
        toastColor = color;
        toastTime = GetTime();
    }

    const std::vector<BodyListEntry> &CurrentListEntries() const {
        return mode == ViewMode::Galaxy ? galaxyListEntries : CurrentSystem().listEntries;
    }

    std::size_t CurrentListSelection() const {
        return mode == ViewMode::Galaxy ? selectedStar : selectedPlanet;
    }

    std::size_t CurrentHoveredBody() const {
        return mode == ViewMode::Galaxy ? hoveredStar : hoveredPlanet;
    }

    bool PanelVisible() const { return !screen.compact || page == CompactPage::Details; }
    bool ListVisible() const { return screen.compact ? page == CompactPage::List : showList; }
    bool MapVisible() const { return !screen.compact || page == CompactPage::Map; }

    void ClampListScroll() {
        const auto &entries = CurrentListEntries();
        BodyListLayout layout = GetBodyListLayout(screen, entries.size());
        listScrollOffset = std::clamp(listScrollOffset, 0.0f,
                                      PanelScroll::MaxOffset(layout.contentHeight, layout.rows.height));
    }

    void EnsureListEntryVisible(std::size_t index) {
        const auto &entries = CurrentListEntries();
        if (index >= entries.size()) return;
        BodyListLayout layout = GetBodyListLayout(screen, entries.size());
        float rowTop = static_cast<float>(index) * layout.rowHeight;
        if (rowTop < listScrollOffset) listScrollOffset = rowTop;
        if (rowTop + layout.rowHeight > listScrollOffset + layout.rows.height)
            listScrollOffset = rowTop + layout.rowHeight - layout.rows.height;
        ClampListScroll();
    }

    void SelectStar(std::size_t index) {
        selectedStar = index;
        panelScrollOffset = 0.0f;
        EnsureListEntryVisible(index);
    }

    void SelectPlanet(std::size_t index) {
        selectedPlanet = index;
        panelScrollOffset = 0.0f;
        EnsureListEntryVisible(index);
    }

    void CycleSelection(int direction) {
        std::size_t count = CurrentListEntries().size();
        if (count == 0) return;
        std::size_t current = CurrentListSelection();
        std::size_t next;
        if (current == kInvalidIndex) next = direction > 0 ? 0 : count - 1;
        else next = (current + count + static_cast<std::size_t>(direction > 0 ? 1 : count - 1)) % count;
        if (mode == ViewMode::Galaxy) SelectStar(next);
        else SelectPlanet(next);
    }

    void FocusPlanet(std::size_t index) {
        const auto &planet = CurrentSystem().planets[index];
        SelectPlanet(index);
        focusedPlanet = index;
        page = CompactPage::Map;
        systemCameraTarget = planet.position;
        cameraController.FocusSystem(camera, systemCameraTarget, planet.radius);
    }

    void ResetCamera() {
        page = CompactPage::Map;
        if (mode == ViewMode::Galaxy) {
            cameraController.ResetGalaxy(camera, true);
            return;
        }
        focusedPlanet = kInvalidIndex;
        systemCameraTarget = Vector3 {};
        cameraController.ResetSystem(camera, CurrentSystem().radius, systemCameraTarget, true);
    }

    void EnterSystem() {
        if (selectedStar == kInvalidIndex) return;
        mode = ViewMode::System;
        page = CompactPage::Map;
        selectedPlanet = kInvalidIndex;
        hoveredPlanet = kInvalidIndex;
        focusedPlanet = kInvalidIndex;
        hoveredListEntry = kInvalidIndex;
        previousClickedPlanet = kInvalidIndex;
        panelScrollOffset = 0.0f;
        listScrollOffset = 0.0f;
        systemCameraTarget = Vector3 {};
        UpdatePlanetPositions();
        cameraController.ResetSystem(camera, CurrentSystem().radius, systemCameraTarget);
    }

    void LeaveSystem() {
        mode = ViewMode::Galaxy;
        page = CompactPage::Map;
        selectedPlanet = kInvalidIndex;
        hoveredPlanet = kInvalidIndex;
        focusedPlanet = kInvalidIndex;
        hoveredListEntry = kInvalidIndex;
        previousClickedStar = kInvalidIndex;
        panelScrollOffset = 0.0f;
        listScrollOffset = 0.0f;
        EnsureListEntryVisible(selectedStar);
    }

    Rectangle BackButtonBounds() const { return {12, 10, 100, 44}; }

    Rectangle EnterButtonBounds() const {
        return {screen.panel.x + kPanelPadding, screen.panel.y + screen.panel.height - 64,
                screen.panel.width - 2 * kPanelPadding, 44};
    }

    Rectangle SystemButtonBounds(bool pause) const {
        float width = (screen.panel.width - 2 * kPanelPadding - 8) * 0.5f;
        return {screen.panel.x + kPanelPadding + (pause ? width + 8 : 0),
                screen.panel.y + screen.panel.height - 106, width, 38};
    }

    Rectangle MapToolBounds(int index) const {
        return {screen.map.width - 166 + index * 52, screen.navigation.y - 50, 50, 44};
    }

    float InputPrefixWidth(const std::string &value, std::size_t length) const {
        std::string prefix = value.substr(0, std::min(length, value.size()));
        return MeasureTextWidth(resources.font, prefix.c_str(), kInputTextSize);
    }

    std::size_t InputPositionAt(const Rectangle &bounds, const std::string &value, float mouseX) const {
        float localX = std::max(0.0f, mouseX - bounds.x - 10.0f);
        float previousWidth = 0.0f;
        std::size_t position = 0;
        for (; position < value.size(); ++position) {
            float nextWidth = InputPrefixWidth(value, position + 1);
            if (localX < (previousWidth + nextWidth) * 0.5f) break;
            previousWidth = nextWidth;
        }
        return position;
    }

    void DrawInputBox(Rectangle bounds, const std::string &value, const char *label, int inputIndex) {
        bool active = activeInput == inputIndex + 1;
        bool hovered = CheckCollisionPointRec(pointer.position, bounds);
        int textY = static_cast<int>(bounds.y + (bounds.height - kInputTextSize) * 0.5f);
        DrawTextLine(resources.font, label, static_cast<int>(bounds.x + 2), 4, 12, theme::kTextDim);
        Color border = theme::kFieldBorder;
        if (invalidInput == inputIndex + 1) border = theme::kError;
        else if (active) border = theme::kAccent;
        else if (hovered) border = Color {110, 150, 185, 255};
        DrawRectangleRounded(bounds, 0.18f, 6, theme::kFieldFill);
        DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, active ? 1.5f : 1.0f, border);
        std::size_t selectionStart = std::min(inputCursor[inputIndex], inputAnchor[inputIndex]);
        std::size_t selectionEnd = std::max(inputCursor[inputIndex], inputAnchor[inputIndex]);
        if (active && selectionStart != selectionEnd) {
            float start = InputPrefixWidth(value, selectionStart);
            float end = InputPrefixWidth(value, selectionEnd);
            DrawRectangle(static_cast<int>(bounds.x + 10.0f + start), static_cast<int>(bounds.y + 6.0f),
                          static_cast<int>(end - start) + 1, static_cast<int>(bounds.height - 12.0f),
                          Color {45, 125, 180, 220});
        }
        DrawTextLine(resources.font, value.c_str(), static_cast<int>(bounds.x + 10.0f), textY, kInputTextSize,
                     theme::kText);
        /* Blink the caret at 1 Hz like a native text field. */
        if (active && std::fmod(GetTime(), 1.0) < 0.6) {
            float cursorX = bounds.x + 10.0f + InputPrefixWidth(value, inputCursor[inputIndex]);
            DrawLineEx(Vector2 {cursorX, bounds.y + 7.0f}, Vector2 {cursorX, bounds.y + bounds.height - 7.0f},
                       1.5f, WHITE);
        }
    }

    void SelectWholeInput(int index) {
        const std::string &input = index == 0 ? seedInput : starCountInput;
        inputAnchor[index] = 0;
        inputCursor[index] = input.size();
    }

    bool ApplyGalaxyInputs() {
        char *end = nullptr;
        long newSeed = std::strtol(seedInput.c_str(), &end, 10);
        if (end == seedInput.c_str() || *end != '\0' || newSeed < 0 || newSeed > 99999999) {
            invalidInput = 1;
            activeInput = 1;
            SelectWholeInput(0);
            ShowToast("Seed must be a number from 0 to 99999999", theme::kError);
            return false;
        }
        long newStarCount = std::strtol(starCountInput.c_str(), &end, 10);
        if (end == starCountInput.c_str() || *end != '\0' || newStarCount < 32 || newStarCount > 64) {
            invalidInput = 2;
            activeInput = 2;
            SelectWholeInput(1);
            ShowToast("Star count must be from 32 to 64", theme::kError);
            return false;
        }
        invalidInput = 0;
        activeInput = 0;
        if (newSeed == seed && newStarCount == starCount) {
            seedInput = std::to_string(seed);
            starCountInput = std::to_string(starCount);
            return true;
        }
        seed = static_cast<int>(newSeed);
        starCount = static_cast<int>(newStarCount);
        pendingRebuild = true;
        busyFrameDrawn = false;
        return true;
    }

    void RandomSeed() {
        static std::mt19937 engine {std::random_device {}()};
        std::uniform_int_distribution<int> distribution(0, 99999999);
        seedInput = std::to_string(distribution(engine));
        ApplyGalaxyInputs();
    }

    void CancelGalaxyInputs() {
        seedInput = std::to_string(seed);
        starCountInput = std::to_string(starCount);
        activeInput = 0;
        invalidInput = 0;
        draggingInput = false;
    }

    void HandleGalaxySettingsInput() {
        Vector2 mouse = pointer.position;
        Rectangle bounds[] = {screen.seed, screen.starCount, screen.apply, screen.random};
        if (pointer.pressed) {
            activeInput = 0;
            draggingInput = false;
            for (int i = 0; i < 4; ++i) {
                if (CheckCollisionPointRec(mouse, bounds[i])) {
                    inputConsumed = true;
                    if (i < 2) {
                        activeInput = i + 1;
                        std::string &input = i == 0 ? seedInput : starCountInput;
                        inputCursor[i] = InputPositionAt(bounds[i], input, mouse.x);
                        inputAnchor[i] = inputCursor[i];
                        draggingInput = true;
                    } else if (i == 2) {
                        ApplyGalaxyInputs();
                    } else {
                        RandomSeed();
                    }
                    break;
                }
            }
        }
        if (activeInput != 0) inputConsumed = true;
        if (draggingInput && pointer.down && activeInput >= 1 && activeInput <= 2) {
            int index = activeInput - 1;
            std::string &input = index == 0 ? seedInput : starCountInput;
            inputCursor[index] = InputPositionAt(bounds[index], input, mouse.x);
        }
        if (!pointer.down) draggingInput = false;
        if (activeInput == 0) return;
        if (IsKeyPressed(KEY_ESCAPE)) {
            CancelGalaxyInputs();
            return;
        }
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (IsKeyPressed(KEY_TAB)) {
            activeInput = activeInput == 1 ? 2 : 1;
            SelectWholeInput(activeInput - 1);
            return;
        }
        int index = activeInput - 1;
        std::string &input = index == 0 ? seedInput : starCountInput;
        std::size_t &cursor = inputCursor[index];
        std::size_t &anchor = inputAnchor[index];
        std::size_t maxLength = index == 0 ? 8 : 2;
        cursor = std::min(cursor, input.size());
        anchor = std::min(anchor, input.size());
        bool left = IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT);
        bool right = IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT);
        bool home = IsKeyPressed(KEY_HOME);
        bool endKey = IsKeyPressed(KEY_END);
        if (left) cursor = cursor > 0 ? cursor - 1 : 0;
        if (right) cursor = std::min(cursor + 1, input.size());
        if (home) cursor = 0;
        if (endKey) cursor = input.size();
        /* Only a caret movement collapses the selection; otherwise Ctrl+A would be undone next frame. */
        if ((left || right || home || endKey) && !shift) anchor = cursor;
        if (control && IsKeyPressed(KEY_A)) anchor = 0, cursor = input.size();
        auto eraseSelection = [&]() {
            std::size_t selectionStart = std::min(cursor, anchor);
            std::size_t selectionEnd = std::max(cursor, anchor);
            if (selectionStart == selectionEnd) return false;
            input.erase(selectionStart, selectionEnd - selectionStart);
            cursor = anchor = selectionStart;
            return true;
        };
        auto insertDigits = [&](const std::string &digits) {
            eraseSelection();
            for (char digit: digits) {
                if (input.size() >= maxLength) break;
                input.insert(cursor, 1, digit);
                ++cursor;
            }
            anchor = cursor;
            invalidInput = 0;
        };
        if (control && IsKeyPressed(KEY_C)) {
            std::size_t selectionStart = std::min(cursor, anchor);
            std::size_t selectionEnd = std::max(cursor, anchor);
            std::string copied = selectionStart == selectionEnd ? input
                                                                : input.substr(selectionStart, selectionEnd - selectionStart);
            SetClipboardText(copied.c_str());
        }
        if (control && IsKeyPressed(KEY_V)) {
            const char *clipboard = GetClipboardText();
            std::string digits;
            for (const char *p = clipboard; p != nullptr && *p != '\0'; ++p)
                if (*p >= '0' && *p <= '9') digits += *p;
            insertDigits(digits);
        }
        bool backspace = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE);
        bool del = IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE);
        if (backspace || del) {
            if (!eraseSelection()) {
                if (backspace && cursor > 0) {
                    input.erase(cursor - 1, 1);
                    --cursor;
                } else if (del && cursor < input.size()) {
                    input.erase(cursor, 1);
                }
                anchor = cursor;
            }
            invalidInput = 0;
        }
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= '0' && key <= '9') insertDigits(std::string(1, static_cast<char>(key)));
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) ApplyGalaxyInputs();
    }

    bool MouseOnPanel() const {
        return PanelVisible() && CheckCollisionPointRec(pointer.position, screen.panel);
    }

    bool MouseOnList() const {
        return ListVisible() && CheckCollisionPointRec(pointer.position,
            GetBodyListLayout(screen, CurrentListEntries().size()).bounds);
    }

    bool PointOnMap(Vector2 point) const {
        if (!MapVisible() || !CheckCollisionPointRec(point, screen.map)) return false;
        if (ListVisible() && CheckCollisionPointRec(point, GetBodyListLayout(screen, CurrentListEntries().size()).bounds))
            return false;
        if (screen.compact)
            for (int i = 0; i < 3; ++i)
                if (CheckCollisionPointRec(point, MapToolBounds(i))) return false;
        return true;
    }

    bool MouseOverUi() const { return !PointOnMap(pointer.position); }

    bool UpdateTouchZoom() {
        int count = GetTouchPointCount();
        if (count < 2) {
            bool finishing = multiTouch;
            if (count == 0) multiTouch = false;
            pinchSpan = 0;
            return finishing;
        }
        Vector2 first = GetTouchPosition(0);
        Vector2 second = GetTouchPosition(1);
        float span = Vector2Distance(first, second);
        if (!multiTouch) {
            pinchOnMap = !showHelp && PointOnMap(first) && PointOnMap(second);
            pressArea = PressArea::None;
            primaryDragging = false;
            movedSincePress = false;
        } else if (pinchOnMap && pinchSpan > 1 && span > 1) {
            cameraController.ScaleDistance(mode, pinchSpan / span);
        }
        multiTouch = true;
        pinchSpan = span;
        return true;
    }

    void PickGalaxyObject() {
        hoveredStar = kInvalidIndex;
        if (MouseOverUi() || IsCursorHidden() || primaryDragging || showHelp) return;
        Vector2 mouse = pointer.position;
        float closest = FLT_MAX;
        for (std::size_t starIndex = 0; starIndex < stars.size(); ++starIndex) {
            const auto &star = stars[starIndex];
            if (!InFrontOfCamera(star.position)) continue;
            float distance = Vector2Distance(GetWorldToScreen(star.position, camera), mouse);
            float hitRadius = std::max(14.0f, ProjectedRadius(star.position, star.radius) + 4.0f);
            if (distance <= hitRadius && distance < closest) {
                closest = distance;
                hoveredStar = starIndex;
            }
        }
    }

    bool InFrontOfCamera(Vector3 position) const {
        Vector3 forward = Vector3Subtract(camera.target, camera.position);
        return Vector3DotProduct(Vector3Subtract(position, camera.position), forward) > 0.0f;
    }

    /* Screen-space radius of a sphere, so tiny planets stay clickable and large ones hit across their disc. */
    float ProjectedRadius(Vector3 position, float radius) const {
        float distance = std::max(0.0001f, Vector3Distance(position, camera.position));
        float pixelsPerUnit = static_cast<float>(GetScreenHeight()) * 0.5f / std::tan(camera.fovy * 0.5f * DEG2RAD);
        return radius / distance * pixelsPerUnit;
    }

    void PickSystemObject() {
        hoveredPlanet = kInvalidIndex;
        if (MouseOverUi() || IsCursorHidden() || primaryDragging || showHelp) return;
        Vector2 mouse = pointer.position;
        float closest = FLT_MAX;
        const auto &planets = CurrentSystem().planets;
        for (std::size_t planetIndex = 0; planetIndex < planets.size(); ++planetIndex) {
            const auto &planet = planets[planetIndex];
            if (!InFrontOfCamera(planet.position)) continue;
            Vector2 screen = GetWorldToScreen(planet.position, camera);
            float distance = Vector3Length(Vector3Subtract(planet.position, camera.position));
            float dx = screen.x - mouse.x;
            float dy = screen.y - mouse.y;
            float hitRadius = std::max(16.0f, ProjectedRadius(planet.position, planet.radius) + 4.0f);
            float screenDistance = dx * dx + dy * dy;
            if (screenDistance <= hitRadius * hitRadius && distance < closest) {
                closest = distance;
                hoveredPlanet = planetIndex;
            }
        }
    }

    std::size_t ListEntryAtMouse() const {
        if (!ListVisible() || IsCursorHidden() || showHelp || movedSincePress) return kInvalidIndex;
        const auto &entries = CurrentListEntries();
        std::size_t hit = BodyListHitTest(GetBodyListLayout(screen, entries.size()), entries.size(),
                                          listScrollOffset, pointer.position);
        return hit < entries.size() ? hit : kInvalidIndex;
    }

    void UpdateHover() {
        hoveredListEntry = ListEntryAtMouse();
        if (mode == ViewMode::Galaxy) {
            PickGalaxyObject();
            if (hoveredListEntry != kInvalidIndex) hoveredStar = hoveredListEntry;
        } else {
            PickSystemObject();
            if (hoveredListEntry != kInvalidIndex) hoveredPlanet = hoveredListEntry;
        }
    }

    // Capture the surface on press so swiping a list or panel never selects a body or orbits the map.
    bool UpdatePrimaryPointer() {
        Vector2 mouse = pointer.position;
        if (pointer.pressed) {
            pressPosition = mouse;
            movedSincePress = false;
            pressArea = PressArea::Control;
            if (!showHelp) {
                if (MouseOnList()) {
                    pressArea = PressArea::List;
                    pressedListEntry = ListEntryAtMouse();
                    pressScrollOffset = listScrollOffset;
                } else if (MouseOnPanel() && mouse.y < screen.panel.y + screen.panel.height - CurrentPanelFooterReserved()) {
                    pressArea = PressArea::Panel;
                    pressScrollOffset = panelScrollOffset;
                } else if (!MouseOverUi()) pressArea = PressArea::Map;
            }
        }
        if (pressArea != PressArea::None && Vector2Distance(mouse, pressPosition) > kDragThreshold)
            movedSincePress = true;
        if (pointer.down) {
            primaryDragging = pressArea == PressArea::Map && movedSincePress;
            float offset = pressScrollOffset + pressPosition.y - mouse.y;
            if (movedSincePress && pressArea == PressArea::List) {
                listScrollOffset = offset;
                ClampListScroll();
            } else if (movedSincePress && pressArea == PressArea::Panel) {
                ScrollPanel(offset, 0);
            }
        } else {
            bool click = pointer.released && !movedSincePress &&
                         ((pressArea == PressArea::Map && !MouseOverUi()) ||
                          (pressArea == PressArea::List && pressedListEntry != kInvalidIndex &&
                           pressedListEntry == ListEntryAtMouse()));
            pressArea = PressArea::None;
            movedSincePress = false;
            primaryDragging = false;
            return click;
        }
        return false;
    }

    void ClickStar(std::size_t index) {
        double now = GetTime();
        bool doubleClick = previousClickedStar == index && now - previousClickTime < kDoubleClickSeconds;
        SelectStar(index);
        previousClickedStar = index;
        previousClickTime = now;
        if (doubleClick) EnterSystem();
    }

    void ClickPlanet(std::size_t index) {
        double now = GetTime();
        bool doubleClick = previousClickedPlanet == index && now - previousClickTime < kDoubleClickSeconds;
        SelectPlanet(index);
        previousClickedPlanet = index;
        previousClickTime = now;
        if (doubleClick) FocusPlanet(index);
    }

    void HandleGalaxyInput(bool click) {
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (IsKeyPressed(KEY_ESCAPE)) {
            selectedStar = kInvalidIndex;
            panelScrollOffset = 0.0f;
            return;
        }
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_S)) &&
            selectedStar != kInvalidIndex) {
            EnterSystem();
            return;
        }
        if (IsKeyPressed(KEY_TAB)) CycleSelection(shift ? -1 : 1);
        if (IsKeyPressed(KEY_B) && !stars.empty()) SelectStar(0);
        if (IsKeyPressed(KEY_R)) {
            RandomSeed();
            return;
        }
        if (PanelVisible() && selectedStar != kInvalidIndex && pointer.pressed &&
            CheckCollisionPointRec(pointer.position, EnterButtonBounds())) {
            EnterSystem();
            return;
        }
        if (!click) return;
        if (hoveredStar == kInvalidIndex) {
            selectedStar = kInvalidIndex;
            panelScrollOffset = 0.0f;
            return;
        }
        ClickStar(hoveredStar);
        if (screen.compact && page == CompactPage::List) page = CompactPage::Details;
    }

    void HandleSystemInput(bool click) {
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool backClicked = pointer.pressed &&
                           CheckCollisionPointRec(pointer.position, BackButtonBounds());
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_G) || backClicked) {
            LeaveSystem();
            return;
        }
        if (IsKeyPressed(KEY_TAB)) CycleSelection(shift ? -1 : 1);
        if ((IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) &&
            selectedPlanet != kInvalidIndex)
            FocusPlanet(selectedPlanet);
        if (IsKeyPressed(KEY_SPACE)) timePaused = !timePaused;
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) SetTimeScale(timeScale * 0.5f);
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) SetTimeScale(timeScale * 2.0f);
        if (PanelVisible() && pointer.pressed) {
            if (CheckCollisionPointRec(pointer.position, SystemButtonBounds(true))) {
                timePaused = !timePaused;
                return;
            }
            if (CheckCollisionPointRec(pointer.position, SystemButtonBounds(false))) {
                if (selectedPlanet != kInvalidIndex && selectedPlanet != focusedPlanet) FocusPlanet(selectedPlanet);
                else ResetCamera();
                page = CompactPage::Map;
                return;
            }
        }
        if (!click) return;
        if (hoveredPlanet == kInvalidIndex) {
            selectedPlanet = kInvalidIndex;
            panelScrollOffset = 0.0f;
            return;
        }
        ClickPlanet(hoveredPlanet);
        if (screen.compact && page == CompactPage::List) page = CompactPage::Details;
    }

    void HandleGlobalKeys() {
        bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_F1)) {
            showHelp = !showHelp;
            inputConsumed = true;
            return;
        }
        if (showHelp) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                showHelp = false;
                inputConsumed = true;
            }
            return;
        }
        if (IsKeyPressed(KEY_L)) {
            if (screen.compact) page = page == CompactPage::List ? CompactPage::Map : CompactPage::List;
            else showList = !showList;
        }
        if (IsKeyPressed(KEY_Z)) ResetCamera();
        if (control && IsKeyPressed(KEY_C)) {
            std::string text = std::to_string(seed) + " " + std::to_string(starCount);
            SetClipboardText(text.c_str());
            ShowToast("Copied \"" + text + "\" (seed, star count)");
        }
    }

    bool HandleCompactNavigation(float &wheel) {
        if (!screen.compact || !pointer.pressed) return false;
        Vector2 mouse = pointer.position;
        for (auto tab : {CompactPage::Map, CompactPage::List, CompactPage::Details}) {
            if (CheckCollisionPointRec(mouse, screen.Tab(tab))) {
                page = tab;
                if (tab == CompactPage::List) EnsureListEntryVisible(CurrentListSelection());
                return true;
            }
        }
        if (!MapVisible()) return false;
        for (int i = 0; i < 3; ++i) {
            if (!CheckCollisionPointRec(mouse, MapToolBounds(i))) continue;
            if (i == 1) ResetCamera();
            else wheel = i == 0 ? -1.0f : 1.0f;
            return true;
        }
        return false;
    }

    void UpdateMouseCursor() {
        Vector2 mouse = pointer.position;
        int cursor = MOUSE_CURSOR_DEFAULT;
        bool overButton = false;
        bool overText = false;
        if (!showHelp) {
            if (mode == ViewMode::Galaxy) {
                overButton = CheckCollisionPointRec(mouse, screen.apply) || CheckCollisionPointRec(mouse, screen.random) ||
                             (PanelVisible() && selectedStar != kInvalidIndex && CheckCollisionPointRec(mouse, EnterButtonBounds()));
                overText = CheckCollisionPointRec(mouse, screen.seed) || CheckCollisionPointRec(mouse, screen.starCount);
            } else {
                overButton = CheckCollisionPointRec(mouse, BackButtonBounds()) ||
                             (PanelVisible() && (CheckCollisionPointRec(mouse, TimeSliderHitBounds()) ||
                              CheckCollisionPointRec(mouse, SystemButtonBounds(true)) ||
                              CheckCollisionPointRec(mouse, SystemButtonBounds(false))));
            }
            if (screen.compact)
                overButton |= CheckCollisionPointRec(mouse, screen.navigation) || (MapVisible() && MouseOverUi() &&
                              CheckCollisionPointRec(mouse, screen.map));
        }
        if (primaryDragging) cursor = MOUSE_CURSOR_RESIZE_ALL;
        else if (overText) cursor = MOUSE_CURSOR_IBEAM;
        else if (overButton || CurrentHoveredBody() != kInvalidIndex) cursor = MOUSE_CURSOR_POINTING_HAND;
        if (cursor != currentCursor) {
            SetMouseCursor(cursor);
            currentCursor = cursor;
        }
    }

    void Update() {
        if (shutdown) return;
        pointer.Poll();
        screen = CalculateViewerLayout(GetScreenWidth(), GetScreenHeight());
        if (IsWindowResized()) ScrollPanel(panelScrollOffset, 0);
        if (pendingRebuild && busyFrameDrawn) {
            pendingRebuild = false;
            busyFrameDrawn = false;
            if (RebuildGalaxy()) ShowToast("Generated seed " + std::to_string(seed) + " with " +
                                           std::to_string(starCount) + " stars");
            else ShowToast("Galaxy generation failed", theme::kError);
            return;
        }
        if (pendingRebuild) return;
        inputConsumed = false;
        float wheel = GetMouseWheelMove();
        double now = GetTime();
        if (lastFrameTime < 0.0) lastFrameTime = now;
        if (!timePaused) simulationTime += std::max(0.0, now - lastFrameTime) * timeScale;
        lastFrameTime = now;

        ClampListScroll();
        if (!showHelp && wheel != 0.0f) {
            if (MouseOnList()) {
                auto layout = GetBodyListLayout(screen, CurrentListEntries().size());
                listScrollOffset = PanelScroll::ApplyWheel(listScrollOffset, wheel, layout.contentHeight, layout.rows.height);
            } else if (MouseOnPanel()) ScrollPanel(panelScrollOffset, wheel);
            if (MouseOverUi()) wheel = 0.0f;
        }
        bool touchZoom = UpdateTouchZoom();
        bool click = !touchZoom && UpdatePrimaryPointer();
        if (click) UpdateHover();
        if (mode == ViewMode::Galaxy && !showHelp && !touchZoom) HandleGalaxySettingsInput();
        bool editingInput = mode == ViewMode::Galaxy && activeInput != 0;
        if (!editingInput && !inputConsumed) HandleGlobalKeys();
        if (showHelp) {
            if (pointer.pressed) showHelp = false;
        } else if (!inputConsumed && !touchZoom && !HandleCompactNavigation(wheel)) {
            UpdateTimeSlider();
            if (mode == ViewMode::Galaxy) HandleGalaxyInput(click);
            else HandleSystemInput(click);
        }
        if (pendingRebuild) return;

        UpdatePlanetPositions();
        if (mode == ViewMode::System && focusedPlanet != kInvalidIndex)
            systemCameraTarget = CurrentSystem().planets[focusedPlanet].position;
#if defined(__EMSCRIPTEN__)
        bool needHide = false;
#else
        bool needHide = !showHelp && MapVisible() && (IsCursorHidden() || !MouseOverUi()) &&
                        (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
#endif
        if (IsCursorHidden() != needHide) {
            if (needHide) DisableCursor();
            else EnableCursor();
        }
        editingInput = mode == ViewMode::Galaxy && activeInput != 0;
        if (!editingInput && !showHelp && MapVisible())
            cameraController.Update(camera, mode, systemCameraTarget, wheel, primaryDragging, pointer.delta,
                                    IsCursorHidden() || !MouseOverUi());
        UpdateHover();
        UpdateMouseCursor();
    }

    const PanelData *CurrentPanel() const {
        if (mode == ViewMode::Galaxy) {
            return selectedStar == kInvalidIndex ? nullptr : &stars[selectedStar].panel;
        }
        if (selectedStar == kInvalidIndex) return nullptr;
        return selectedPlanet == kInvalidIndex ? &stars[selectedStar].panel
                                                : &CurrentSystem().planets[selectedPlanet].panel;
    }

    int CurrentPanelFooterReserved() const {
        return mode == ViewMode::Galaxy ? kGalaxyPanelFooterReserved : kSystemPanelFooterReserved;
    }

    void ScrollPanel(float offset, float wheel) {
        const PanelData *panel = CurrentPanel();
        if (!panel) return;
        auto layout = GetViewerPanelLayout(resources.font, *panel, screen.panel, CurrentPanelFooterReserved());
        panelScrollOffset = PanelScroll::ApplyWheel(offset, wheel, layout.contentHeight, layout.scrollBounds.height);
    }

    void DrawTopBar() {
        int width = static_cast<int>(screen.topBar.width);
        int height = static_cast<int>(kTopBarHeight);
        DrawRectangleGradientV(0, 0, width, height, Color {5, 12, 23, 238}, Color {5, 12, 23, 205});
        DrawLine(0, height, width, height, theme::kSeparator);
        if (mode == ViewMode::Galaxy) {
            if (screen.seed.x >= 200) {
                DrawTextLine(resources.font, "GALAXY MAP", 20, 10, 22, theme::kAccentSoft);
                if (MeasureTextWidth(resources.font, galaxySummary.c_str(), 13) < screen.seed.x - 28)
                    DrawTextLine(resources.font, galaxySummary.c_str(), 20, 38, 13, theme::kTextDim);
            }
            DrawInputBox(screen.seed, seedInput, "SEED", 0);
            DrawInputBox(screen.starCount, starCountInput, "STARS", 1);
            DrawViewerButton(resources.font, screen.apply, "Apply", true);
            DrawViewerButton(resources.font, screen.random, "Random");
            return;
        }
        const auto &system = CurrentSystem();
        DrawViewerButton(resources.font, BackButtonBounds(), "<  GALAXY");
        DrawHeading(resources.font, stars[selectedStar].data->name, 124, 10, 20, width - 132, theme::kAccentSoft);
        std::string summary = focusedPlanet != kInvalidIndex ? "Following " + system.planets[focusedPlanet].name
                                                             : system.summary;
        if (MeasureTextWidth(resources.font, summary.c_str(), 13) > width - 132)
            summary = std::to_string(system.planets.size()) + " planets";
        DrawTextLine(resources.font, summary.c_str(), 124, 38, 13, theme::kTextDim);
    }

    void DrawGalaxyOverlay() {
        int x = static_cast<int>(screen.panel.x + kPanelPadding);
        int top = static_cast<int>(screen.panel.y);
        if (selectedStar != kInvalidIndex) {
            DrawViewerPanel(resources.font, stars[selectedStar].panel, screen.panel, panelScrollOffset,
                            CurrentPanelFooterReserved());
        } else {
            DrawPanelBackground(screen.panel);
            DrawTextLine(resources.font, "STAR PARAMETERS", x, top + 22, 13, theme::kAccentSoft);
            DrawTextLine(resources.font, "No star selected", x, top + 40, 26, theme::kText);
            DrawTextLine(resources.font, "Choose a star on the map or in the list.", x, top + 86, 15, theme::kTextDim);
        }
        DrawViewerButton(resources.font, EnterButtonBounds(), screen.compact ? "Enter system" : "Enter system   [Enter]", true,
                         selectedStar != kInvalidIndex);
    }

    void DrawSystemOverlay() {
        if (selectedPlanet != kInvalidIndex)
            DrawViewerPanel(resources.font, CurrentSystem().planets[selectedPlanet].panel, screen.panel,
                            panelScrollOffset, CurrentPanelFooterReserved());
        else
            DrawViewerPanel(resources.font, stars[selectedStar].panel, screen.panel, panelScrollOffset,
                            CurrentPanelFooterReserved());
        DrawViewerButton(resources.font, SystemButtonBounds(false),
                         selectedPlanet != kInvalidIndex && selectedPlanet != focusedPlanet ? "Follow planet" : "Center on star");
        DrawViewerButton(resources.font, SystemButtonBounds(true), timePaused ? "Resume" : "Pause");
        int x = static_cast<int>(screen.panel.x + kPanelPadding);
        Rectangle slider = TimeSliderBounds();
        int labelY = static_cast<int>(slider.y - 28.0f);
        DrawTextLine(resources.font, "ORBIT SPEED", x, labelY, 13, theme::kLabel);
        char speedText[32];
        if (timeScale < 1.0f) std::snprintf(speedText, sizeof(speedText), "%.2fx", timeScale);
        else if (timeScale < 10.0f) std::snprintf(speedText, sizeof(speedText), "%.1fx", timeScale);
        else std::snprintf(speedText, sizeof(speedText), "%.0fx", timeScale);
        DrawTextRight(resources.font, speedText, static_cast<int>(slider.x + slider.width), labelY, 13,
                      timePaused ? theme::kWarn : theme::kText);
        float fraction = std::clamp(TimeScaleFraction(timeScale), 0.0f, 1.0f);
        Color fill = timePaused ? theme::kFieldBorder : theme::kAccent;
        DrawRectangleRounded(slider, 1.0f, 6, Color {25, 43, 61, 255});
        DrawRectangleRounded(Rectangle {slider.x, slider.y, std::max(slider.height, slider.width * fraction),
                                        slider.height},
                             1.0f, 6, fill);
        float realTimeX = slider.x + slider.width * TimeScaleFraction(1.0f);
        DrawLineEx(Vector2 {realTimeX, slider.y + slider.height + 3.0f},
                   Vector2 {realTimeX, slider.y + slider.height + 7.0f}, 1.0f, theme::kHint);
        bool hovered = draggingTimeSlider || CheckCollisionPointRec(pointer.position, TimeSliderHitBounds());
        Vector2 handle {slider.x + slider.width * fraction, slider.y + slider.height * 0.5f};
        DrawCircleV(handle, hovered ? 8.0f : 7.0f, WHITE);
        DrawCircleV(handle, 3.0f, fill);
    }

    void DrawMapMarkers() {
        if (mode == ViewMode::Galaxy) {
            if (hoveredStar == kInvalidIndex || hoveredStar == selectedStar) return;
            const auto &star = stars[hoveredStar];
            if (!InFrontOfCamera(star.position)) return;
            Vector2 screen = GetWorldToScreen(star.position, camera);
            DrawRing(screen, 10.0f, 11.5f, 0.0f, 360.0f, 40, Color {137, 221, 255, 190});
            return;
        }
        // Keep the selection outline thin at every zoom and reserve room for the body when placing names.
        auto mark = [&](Vector3 position, float worldRadius, bool selected, bool hovered) {
            if (!InFrontOfCamera(position)) return;
            Vector2 center = GetWorldToScreen(position, camera);
            float radius = ProjectedRadius(position, worldRadius);
            if (selected || hovered) {
                radius = std::max(9.0f, radius + (hovered && !selected ? 5.0f : 0.0f));
                int segments = static_cast<int>(std::clamp(radius, 40.0f, 192.0f));
                DrawRing(center, radius, radius + 1.5f, 0, 360, segments,
                         selected ? Color{238, 248, 255, 235} : theme::kAccentSoft);
            }
            labelBounds.push_back({center.x - radius, center.y - radius, 2 * radius, 2 * radius});
        };
        const auto &system = CurrentSystem();
        bool starSelected = selectedPlanet == kInvalidIndex;
        float starScale = sceneRenderer.StarVisualScale(system.star, camera);
        mark(system.star.position, starScale * (starSelected ? 2.2f : 0.5f), starSelected, false);
        for (std::size_t i = 0; i < system.planets.size(); ++i) {
            const auto &planet = system.planets[i];
            bool selected = i == selectedPlanet;
            mark(planet.position, planet.radius * (selected ? 1.6f : 1.0f), selected, i == hoveredPlanet);
        }
    }

    void DrawOverlays() {
        DrawTopBar();
        if (ListVisible()) {
            const auto &entries = CurrentListEntries();
            DrawBodyList(resources.font, GetBodyListLayout(screen, entries.size()),
                         mode == ViewMode::Galaxy ? "STARS" : "PLANETS", entries, listScrollOffset,
                         CurrentListSelection(), CurrentHoveredBody());
        }
        if (PanelVisible()) {
            if (mode == ViewMode::Galaxy) DrawGalaxyOverlay();
            else DrawSystemOverlay();
        }
        if (screen.compact) {
            DrawRectangleRec(screen.navigation, theme::kPanelFill);
            DrawLine(0, static_cast<int>(screen.navigation.y), GetScreenWidth(),
                     static_cast<int>(screen.navigation.y), theme::kSeparator);
            for (auto tab : {CompactPage::Map, CompactPage::List, CompactPage::Details}) {
                Rectangle bounds = screen.Tab(tab);
                const char *label = "Map";
                if (tab == CompactPage::List) label = mode == ViewMode::Galaxy ? "Stars" : "Planets";
                if (tab == CompactPage::Details) label = "Details";
                if (page == tab) {
                    DrawRectangleRec(bounds, theme::kRowSelected);
                    DrawRectangleRec({bounds.x, bounds.y, bounds.width, 2}, theme::kAccent);
                }
                int x = static_cast<int>(bounds.x + (bounds.width - MeasureTextWidth(resources.font, label, 16)) * 0.5f);
                DrawTextLine(resources.font, label, x, static_cast<int>(bounds.y + 15), 16,
                             page == tab ? WHITE : theme::kTextDim);
            }
            if (MapVisible()) {
                DrawViewerButton(resources.font, MapToolBounds(0), "-");
                DrawViewerButton(resources.font, MapToolBounds(1), "Reset");
                DrawViewerButton(resources.font, MapToolBounds(2), "+");
            }
        } else {
            DrawHintBar(resources.font, screen.map.width < 850
                ? "Drag  orbit     Wheel  zoom     Tab  select     H  help"
                : mode == ViewMode::Galaxy
                    ? "Drag  orbit     Wheel  zoom     Tab  next star     Enter  open system     H  help"
                    : "Drag  orbit     Right drag  roll     Wheel  zoom     F  follow     Space  pause     H  help");
        }
        double toastAge = GetTime() - toastTime;
        if (!toastText.empty() && toastAge < kToastSeconds) {
            float alpha = static_cast<float>(std::min(1.0, (kToastSeconds - toastAge) / 0.4));
            DrawToast(resources.font, screen.map, toastText.c_str(), toastColor, alpha);
        }
        if (showHelp) DrawHelpOverlay(resources.font, mode);
        if (pendingRebuild) {
            DrawBusyOverlay(resources.font, "Generating galaxy...");
            busyFrameDrawn = true;
        }
    }

    void DrawSceneLabel(const char *text, float width, Vector2 body, Vector2 offset, Color color) {
        if (!PointOnMap(body)) return;
        Rectangle bounds{body.x + offset.x, body.y + offset.y, width, 16};
        if (!PlaceSceneLabel(bounds, body, screen.map, labelBounds)) return;
        labelBounds.push_back(bounds);
        Vector2 position{bounds.x, bounds.y};
        DrawTextEx(resources.font, text, Vector2 {position.x + 1.0f, position.y + 1.0f}, 16.0f, 1.0f,
                   Color {0, 0, 0, 160});
        DrawTextEx(resources.font, text, position, 16.0f, 1.0f, color);
    }

    // `StarData.systemRadius` is the outermost planet's sun distance, or the dyson radius when the
    // star has no planets.
    static float SystemRadius(const dspugen::Star *star) {
        if (star->planets.empty()) return star->dysonRadius;
        return star->planets.back()->sunDistance;
    }

    // `UIStarmapPlanet._OnLateUpdate` offsets the name by `(5 + 420 / num, 6)` where `num` is the
    // camera distance over the planet's world diameter, and drops the label past
    // `systemRadius * 40000 * 7 * 0.00025`. `UIStarmapStar` uses `(8 + 600 / num, 4)` and a limit of
    // `systemRadius * 40000 * 12 * 0.00025`. Lay out selected and hovered labels first.
    void DrawSystemLabels() {
        const auto &system = CurrentSystem();
        float systemRadius = SystemRadius(stars[selectedStar].data);
        auto drawPlanet = [&](std::size_t i, Color color) {
            if (i == kInvalidIndex) return;
            const auto &planet = system.planets[i];
            float distance = Vector3Distance(planet.position, camera.position);
            if (distance > systemRadius * 10.0f * 7.0f || !InFrontOfCamera(planet.position)) return;
            float scale = std::max(1.0f, distance / std::max(0.0001f, planet.radius * 2.0f));
            DrawSceneLabel(planet.name.c_str(), planet.labelWidth, GetWorldToScreen(planet.position, camera),
                           {5.0f + 420.0f / scale, -6}, color);
        };
        drawPlanet(selectedPlanet, theme::kAccentSoft);
        if (hoveredPlanet != selectedPlanet) drawPlanet(hoveredPlanet, WHITE);
        float starDistance = Vector3Length(Vector3Subtract(system.star.position, camera.position));
        if (starDistance < systemRadius * 10.0f * 12.0f && InFrontOfCamera(system.star.position)) {
            float scale = std::max(1.0f, starDistance /
                                             std::max(0.0001f, sceneRenderer.StarVisualScale(system.star, camera)));
            DrawSceneLabel(system.star.data->name.c_str(), system.star.labelWidth, GetWorldToScreen(system.star.position, camera),
                           {8.0f + 600.0f / scale, -4},
                           selectedPlanet == kInvalidIndex ? theme::kAccentSoft : LIGHTGRAY);
        }
        for (std::size_t i = 0; i < system.planets.size(); ++i)
            if (i != selectedPlanet && i != hoveredPlanet) drawPlanet(i, LIGHTGRAY);
    }

    void DrawHoverLabel() {
        if (ListVisible()) labelBounds.push_back(GetBodyListLayout(screen, CurrentListEntries().size()).bounds);
        if (screen.compact) {
            for (int i = 0; i < 3; ++i) labelBounds.push_back(MapToolBounds(i));
        } else {
            labelBounds.push_back({0, static_cast<float>(GetScreenHeight()) - kHintBarHeight - kBodyListMargin,
                                   screen.map.width, kHintBarHeight});
        }
        if (mode == ViewMode::Galaxy) {
            auto drawStar = [&](std::size_t i, Color color) {
                if (i == kInvalidIndex || !InFrontOfCamera(stars[i].position)) return;
                const auto &star = stars[i];
                DrawSceneLabel(star.data->name.c_str(), star.labelWidth, GetWorldToScreen(star.position, camera),
                               {18, -6}, color);
            };
            drawStar(selectedStar, theme::kAccentSoft);
            if (hoveredStar != selectedStar) drawStar(hoveredStar, WHITE);
            if (!stars.empty() && selectedStar != 0 && hoveredStar != 0) drawStar(0, LIGHTGRAY);
            return;
        }
        DrawSystemLabels();
    }

    void Draw() {
        BeginDrawing();
        // Galaxy Select uses near 0.2 / far 10000, which the 5000 unit background cube needs. The
        // Starmap Camera serialises near 0.02 / far 150000 and nothing overrides it at runtime; the
        // 50000 unit background shells and the rest of the cluster need that range.
        if (mode == ViewMode::Galaxy) rlSetClipPlanes(0.2, 10000.0);
        else rlSetClipPlanes(0.02, 150000.0);
        ClearBackground(Color {4, 8, 17, 255});
        if (MapVisible()) postProcess.Draw(camera, resources, this, DrawSceneCallback);
        /* The scene pass can leave depth testing on; 2D overlay batches restart their depth on every
           flush (for example at a clip boundary) and would then fail against the copied scene depth. */
        rlDrawRenderBatchActive();
        rlDisableDepthTest();
        if (MapVisible()) {
            labelBounds.clear();
            DrawMapMarkers();
            DrawHoverLabel();
        }
        DrawOverlays();
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
        postProcess.Unload();
        UnloadRenderResources(resources);
        CloseWindow();
    }
};

Viewer *Viewer::current = nullptr;

#if defined(__EMSCRIPTEN__)
// Open the native editor during the touch event so mobile browsers can display their keyboard.
extern "C" EMSCRIPTEN_KEEPALIVE int viewerBeginNumberEdit(float x, float y) {
    auto *view = Viewer::current;
    if (!view || view->mode != ViewMode::Galaxy || view->pendingRebuild || view->showHelp) return 0;
    Vector2 point{x * GetScreenWidth(), y * GetScreenHeight()};
    bool seed = CheckCollisionPointRec(point, view->screen.seed);
    if (!seed && !CheckCollisionPointRec(point, view->screen.starCount)) return 0;
    Rectangle bounds = seed ? view->screen.seed : view->screen.starCount;
    view->activeInput = seed ? 1 : 2;
    EM_ASM({
        Module.openNumberEditor(UTF8ToString($0), $1, $2, $3, $4, $5);
    }, (seed ? view->seedInput : view->starCountInput).c_str(), seed,
       bounds.x / GetScreenWidth(), bounds.y / GetScreenHeight(),
       bounds.width / GetScreenWidth(), bounds.height / GetScreenHeight());
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE void viewerFinishNumberEdit(int submit) {
    auto *view = Viewer::current;
    if (!view || view->activeInput == 0) return;
    char value[9]{};
    EM_ASM({ stringToUTF8(document.getElementById('number-editor').value, $0, $1); }, value, sizeof(value));
    (view->activeInput == 1 ? view->seedInput : view->starCountInput) = value;
    view->activeInput = 0;
    view->invalidInput = 0;
    if (submit < 0) view->CancelGalaxyInputs();
    else if (submit > 0) view->ApplyGalaxyInputs();
}
#endif

int RunViewer(int argc, char *argv[]) {
    // --fxaa and --bloom mirror the game's FXAA and BloomLevel video options, so a capture can be
    // reproduced under the same post-effect configuration. Defaults match the game's own defaults.
    bool fxaa = true;
    int bloomLevel = 2;
    // --orbit-width scales the drawn orbit line against the game's own width. The viewer defaults
    // to half so the lines stop hiding the planets sitting on them; 1 reproduces the game.
    float orbitWidth = 0.5f;
    const char *positional[3] = {nullptr, nullptr, nullptr};
    int positionalCount = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--fxaa=", 7) == 0) {
            fxaa = std::atoi(argv[i] + 7) != 0;
        } else if (std::strncmp(argv[i], "--bloom=", 8) == 0) {
            bloomLevel = std::atoi(argv[i] + 8);
        } else if (std::strncmp(argv[i], "--orbit-width=", 14) == 0) {
            orbitWidth = static_cast<float>(std::atof(argv[i] + 14));
        } else if (positionalCount < 3) {
            positional[positionalCount++] = argv[i];
        }
    }
    int seed = positional[0] != nullptr ? std::atoi(positional[0]) : 0;
    int starCount = positional[1] != nullptr ? std::atoi(positional[1]) : 64;
    starCount = std::clamp(starCount, 1, 256);
    Viewer viewer(seed, starCount, positional[2] != nullptr ? positional[2] : DSP_VIEWER_DEFAULT_FONT);
    if (!viewer.Init()) return 1;
    viewer.SetPostEffects(fxaa, bloomLevel);
    viewer.SetOrbitWidth(orbitWidth);
    viewer.Run();
    return 0;
}

}
