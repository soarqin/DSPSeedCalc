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
#include "post_process.hh"
#include "render_resources.hh"
#include "scene_renderer.hh"
#include "view_model.hh"
#include "view_model_builder.hh"
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

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;
constexpr float kPanelWidth = 326.0f;
constexpr float kGalaxyGridHeight = -15.0f;
constexpr float kSystemOrbitScale = 10.0f;
constexpr int kOrbitSegments = 72;
constexpr int kEnterButtonTextWidth = 243;
constexpr int kBackButtonTextWidth = 80;
constexpr int kViewerFontSize = 64;
constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

class Viewer {
public:
    Viewer(int seed, int starCount, std::string fontPath)
        : seed(seed), starCount(starCount), fontPath(std::move(fontPath)),
          seedInput(std::to_string(seed)), starCountInput(std::to_string(starCount)),
          sceneRenderer(resources, postProcess) {}

    bool Init() {
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(kScreenWidth, kScreenHeight, "DSPSeedCalc Universe Viewer");
        if (!LoadRenderResources(resources, fontPath)) {
            Shutdown();
            return false;
        }
        SetTargetFPS(60);
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
        galaxyGridLines.clear();
        selectedStar = kInvalidIndex;
        hoveredStar = kInvalidIndex;
        selectedPlanet = kInvalidIndex;
        hoveredPlanet = kInvalidIndex;
        previousClickedStar = kInvalidIndex;
        previousClickedPlanet = kInvalidIndex;
        previousClickTime = -1.0;
        mode = ViewMode::Galaxy;
        simulationTime = 0.0;
        lastFrameTime = GetTime();

        galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, seed, starCount);
        if (galaxy == nullptr) return false;
        galaxy->ensureAllPlanets();

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
            view.labelWidth = MeasureTextEx(resources.font, star->name.c_str(), 16.0f, 1.0f).x;
            view.panel = CreateStarPanel(star, seed);
            view.massMaterial = StarMassMaterial(star);
            view.spotMaterial = StarSpotMaterial(star);
            stars.emplace_back(std::move(view));
        }
        galaxyLines = CreateGalaxyLines(galaxy, stars);
        for (std::size_t i = 0; i < stars.size(); ++i) BuildSystemCache(i);
        char summary[96];
        std::snprintf(summary, sizeof(summary), "seed %d  /  %d stars  /  birth star #%d",
                      seed, galaxy->starCount, galaxy->birthStarId);
        galaxySummary = summary;
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
    std::vector<StarViewData> stars;
    std::vector<SystemViewCache> systems;
    std::vector<GalaxyLineData> galaxyLines;
    std::vector<Line3DCache> galaxyGridLines;
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
    Vector3 systemCameraTarget{};
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

    static void DrawSceneCallback(const void *context, float time) {
        auto &viewer = *const_cast<Viewer *>(static_cast<const Viewer *>(context));
        if (viewer.mode == ViewMode::Galaxy)
            viewer.sceneRenderer.DrawGalaxy(viewer.galaxyGridLines, viewer.galaxyLines,
                                            viewer.stars, viewer.camera, time);
        else
            viewer.sceneRenderer.DrawSystem(viewer.CurrentSystem(), viewer.camera, time,
                                            viewer.selectedPlanet, kInvalidIndex);
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
            view.selectionRadius = radius * 1.35f;
            view.parentIndex = parentIndex;
            view.orbitRadius = orbitRadius;
            view.orbitInclination = inclination;
            view.orbitLongitude = longitude;
            view.orbitPhase = angle;
            view.orbitalPeriod = std::max(orbit.period, 1.0);
            view.orbitRotation = orbitRotation;
            view.orbitPoints.reserve(kOrbitSegments + 1);
            for (int i = 0; i <= kOrbitSegments; ++i) {
                float orbitAngle = static_cast<float>(i) / kOrbitSegments * 2.0f * PI;
                view.orbitPoints.push_back(OrbitPoint(Vector3 {}, orbitRadius, orbitAngle, orbitRotation));
            }
            view.name = PlanetName(planet);
            view.labelWidth = MeasureTextEx(resources.font, view.name.c_str(), 16.0f, 1.0f).x;
            view.panel = CreatePlanetPanel(planet, view.name);
            view.thumbMaterial = PlanetThumbMaterial(planet);
            system.radius = std::max(system.radius, Vector3Length(position) + radius);
            system.planets.emplace_back(std::move(view));
        }
        system.star = starView;
        system.star.position = Vector3 {};
        system.star.radius = sceneRenderer.StarViewRadius(starView.data);
        system.star.selectionSize = Vector3 {system.star.radius * 1.8f, system.star.radius * 1.8f,
                                             system.star.radius * 1.8f};
        char summary[96];
        std::snprintf(summary, sizeof(summary), "system view  /  %d planets  /  click a planet for details",
                      static_cast<int>(system.planets.size()));
        system.summary = summary;
    }

    void UpdatePlanetPositions(double now) {
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
        if (selectedStar == kInvalidIndex) return;
        mode = ViewMode::System;
        selectedPlanet = kInvalidIndex;
        hoveredPlanet = kInvalidIndex;
        float systemRadius = CurrentSystem().radius;
        systemCameraTarget = Vector3 {};
        cameraController.ResetSystem(camera, systemRadius, systemCameraTarget);
    }

    Rectangle SeedInputBounds() const { return Rectangle {430.0f, 14.0f, 150.0f, 32.0f}; }
    Rectangle StarCountInputBounds() const { return Rectangle {642.0f, 14.0f, 86.0f, 32.0f}; }
    Rectangle ApplyInputBounds() const { return Rectangle {738.0f, 14.0f, 92.0f, 32.0f}; }

    std::size_t InputPositionAt(const Rectangle &bounds, const std::string &value, float mouseX) const {
        float localX = std::max(0.0f, mouseX - bounds.x - 9.0f);
        float previousWidth = 0.0f;
        std::size_t position = 0;
        for (; position < value.size(); ++position) {
            char glyph[2] {value[position], '\0'};
            float glyphWidth = MeasureTextEx(resources.font, glyph, 17.0f, 1.0f).x;
            float nextWidth = previousWidth + glyphWidth;
            if (localX < (previousWidth + nextWidth) * 0.5f) break;
            previousWidth = nextWidth;
        }
        return position;
    }

    float InputPrefixWidth(const std::string &value, std::size_t length) const {
        float width = 0.0f;
        for (std::size_t i = 0; i < std::min(length, value.size()); ++i) {
            char glyph[2] {value[i], '\0'};
            width += MeasureTextEx(resources.font, glyph, 17.0f, 1.0f).x;
        }
        return width;
    }

    void DrawInputBox(Rectangle bounds, const std::string &value, const char *label, int inputIndex) {
        bool active = activeInput == inputIndex + 1;
        DrawTextLine(resources.font, label, static_cast<int>(bounds.x), static_cast<int>(bounds.y - 12.0f), 11,
                 Color {123, 157, 183, 255});
        DrawRectangleRounded(bounds, 0.16f, 6, Color {19, 31, 48, 235});
        DrawRectangleRoundedLinesEx(bounds, 0.16f, 6, 1.0f,
                                    active ? Color {88, 198, 255, 255} : Color {72, 104, 133, 255});
        std::size_t selectionStart = std::min(inputCursor[inputIndex], inputAnchor[inputIndex]);
        std::size_t selectionEnd = std::max(inputCursor[inputIndex], inputAnchor[inputIndex]);
        if (active && selectionStart != selectionEnd) {
            float start = InputPrefixWidth(value, selectionStart);
            float end = InputPrefixWidth(value, selectionEnd);
            DrawRectangle(static_cast<int>(bounds.x + 8.0f + start), static_cast<int>(bounds.y + 5.0f),
                          static_cast<int>(end - start), 22, Color {45, 125, 180, 220});
        }
        DrawTextLine(resources.font, value.c_str(), static_cast<int>(bounds.x + 9.0f), static_cast<int>(bounds.y + 7.0f), 17, WHITE);
        if (active) {
            float cursorX = InputPrefixWidth(value, inputCursor[inputIndex]);
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
        selectedPlanet = kInvalidIndex;
        hoveredPlanet = kInvalidIndex;
        cameraController.ResetGalaxy(camera);
    }

    bool MouseOnPanel() const {
        return GetMousePosition().x >= static_cast<float>(GetScreenWidth()) - kPanelWidth;
    }

    void PickGalaxyObject() {
        hoveredStar = kInvalidIndex;
        if (MouseOnPanel() || IsCursorHidden()) return;
        Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
        Vector3 rayEnd = ray.position + ray.direction * 300.0f;
        float closest = 1.7f;
        for (std::size_t starIndex = 0; starIndex < stars.size(); ++starIndex) {
            const auto &star = stars[starIndex];
            Vector3 raySpan = rayEnd - ray.position;
            float squaredLength = Vector3LengthSqr(raySpan);
            float parameter = squaredLength < 0.000001f ? 0.0f :
                              Vector3DotProduct(star.position - ray.position, raySpan) / squaredLength;
            float distance = Vector3Distance(ray.position + raySpan * parameter, star.position);
            if (distance < closest) {
                closest = distance < star.galaxyScale * 0.25f ? 0.0f : distance;
                hoveredStar = starIndex;
            }
        }
    }

    void PickSystemObject() {
        hoveredPlanet = kInvalidIndex;
        if (MouseOnPanel() || IsCursorHidden()) return;
        Vector2 mouse = GetMousePosition();
        float closest = FLT_MAX;
        const auto &planets = CurrentSystem().planets;
        for (std::size_t planetIndex = 0; planetIndex < planets.size(); ++planetIndex) {
            const auto &planet = planets[planetIndex];
            Vector2 screen = GetWorldToScreen(planet.position, camera);
            float distance = Vector3Length(Vector3Subtract(planet.position, camera.position));
            float dx = screen.x - mouse.x;
            float dy = screen.y - mouse.y;
            float screenDistance = dx * dx + dy * dy;
            if (screenDistance <= 14.0f * 14.0f && distance < closest) {
                closest = distance;
                hoveredPlanet = planetIndex;
            }
        }
    }

    void HandleGalaxyInput() {
        if (activeInput != 0) return;
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_S)) && selectedStar != kInvalidIndex) {
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
        if (hoveredStar == kInvalidIndex) {
            selectedStar = kInvalidIndex;
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
        if (hoveredPlanet == kInvalidIndex) {
            selectedPlanet = kInvalidIndex;
            return;
        }
        double now = GetTime();
        bool doubleClick = previousClickedPlanet == hoveredPlanet && now - previousClickTime < 0.34;
        selectedPlanet = hoveredPlanet;
        previousClickedPlanet = hoveredPlanet;
        previousClickTime = now;
        if (doubleClick) {
            const auto &planet = CurrentSystem().planets[hoveredPlanet];
            systemCameraTarget = planet.position;
            cameraController.FocusSystem(camera, systemCameraTarget, planet.radius);
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
        if (!editingInput)
            cameraController.Update(camera, mode, systemCameraTarget);
        if (mode == ViewMode::Galaxy) HandleGalaxySettingsInput();
        UpdateTimeSlider();
        UpdatePlanetPositions(now);
        if (!editingInput && !inputConsumed && IsKeyPressed(KEY_Z)) {
            if (mode == ViewMode::Galaxy) {
                cameraController.ResetGalaxy(camera);
            } else {
                float systemRadius = CurrentSystem().radius;
                systemCameraTarget = Vector3 {};
                cameraController.ResetSystem(camera, systemRadius, systemCameraTarget);
            }
        }

        if (mode == ViewMode::Galaxy) {
            PickGalaxyObject();
            if (activeInput == 0 && !inputConsumed) HandleGalaxyInput();
        } else {
            PickSystemObject();
            HandleSystemInput();
        }
    }

    void DrawTopBar() {
        DrawRectangle(0, 0, GetScreenWidth() - static_cast<int>(kPanelWidth), 62,
                      Color {5, 12, 23, 218});
        if (mode == ViewMode::Galaxy) {
            DrawTextLine(resources.font, "GALAXY MAP", 24, 17, 22, Color {177, 232, 255, 255});
            DrawTextLine(resources.font, galaxySummary.c_str(), 24, 42, 14, Color {121, 154, 179, 255});
            DrawInputBox(SeedInputBounds(), seedInput, "seed 0-99999999", 0);
            DrawInputBox(StarCountInputBounds(), starCountInput, "stars 32-64", 1);
            DrawViewerButton(resources.font, ApplyInputBounds(), "APPLY", 46, true);
        } else {
            DrawViewerButton(resources.font, Rectangle {22.0f, 20.0f, 112.0f, 38.0f}, "BACK  [G]", kBackButtonTextWidth);
            DrawTextLine(resources.font, stars[selectedStar].data->name.c_str(), 160, 17, 22, Color {177, 232, 255, 255});
            DrawTextLine(resources.font, CurrentSystem().summary.c_str(), 160, 42, 14, Color {121, 154, 179, 255});
        }
    }

    void DrawGalaxyOverlay() {
        int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
        if (selectedStar != kInvalidIndex) {
            DrawViewerPanel(resources.font, stars[selectedStar].panel);
        } else {
            DrawPanelBackground();
            DrawTextLine(resources.font, "SELECT A STAR", x, 94, 20, Color {137, 221, 255, 255});
            DrawTextLine(resources.font, "Left click a star to inspect", x, 132, 16, Color {178, 197, 213, 255});
            DrawTextLine(resources.font, "its generated parameters.", x, 155, 16, Color {178, 197, 213, 255});
            DrawTextLine(resources.font, "Press Enter or double click", x, 203, 16, Color {178, 197, 213, 255});
            DrawTextLine(resources.font, "to enter the planetary system.", x, 226, 16, Color {178, 197, 213, 255});
        }
        DrawViewerButton(resources.font, Rectangle {static_cast<float>(x), GetScreenHeight() - 104.0f, kPanelWidth - 44.0f, 38.0f},
                   "ENTER SYSTEM  [ENTER]", kEnterButtonTextWidth, selectedStar != kInvalidIndex);
        DrawTextLine(resources.font, "MMB orbit  /  wheel zooms around center", x, GetScreenHeight() - 54, 14,
                  Color {98, 131, 159, 255});
        DrawTextLine(resources.font, "Arrows rotate  /  Z reset", x, GetScreenHeight() - 33, 14, Color {98, 131, 159, 255});
    }

    void DrawSystemOverlay() {
        if (selectedPlanet != kInvalidIndex) DrawViewerPanel(resources.font, CurrentSystem().planets[selectedPlanet].panel);
        else DrawViewerPanel(resources.font, stars[selectedStar].panel);
        int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
        Rectangle slider = TimeSliderBounds();
        DrawRectangleRounded(slider, 0.5f, 4, Color {25, 43, 61, 255});
        float fraction = timeScale / 100.0f;
        DrawRectangleRounded(Rectangle {slider.x, slider.y, slider.width * fraction, slider.height},
                             0.5f, 4, Color {88, 198, 255, 255});
        DrawCircle(static_cast<int>(slider.x + slider.width * fraction),
                   static_cast<int>(slider.y + slider.height * 0.5f), 7.0f, WHITE);
        char timeScaleText[32];
        std::snprintf(timeScaleText, sizeof(timeScaleText), "time speed  %.1fx", timeScale);
        DrawTextLine(resources.font, timeScaleText, x, slider.y - 22, 14,
                 Color {123, 157, 183, 255});
        DrawTextLine(resources.font, "MMB orbit  /  RMB roll  /  wheel zoom", x, GetScreenHeight() - 54, 14,
                 Color {98, 131, 159, 255});
        DrawTextLine(resources.font, "Double click focuses  /  ESC or G returns", x, GetScreenHeight() - 33, 14,
                 Color {98, 131, 159, 255});
    }

    void DrawHoverLabel() {
        if (IsCursorHidden()) return;
        if (mode == ViewMode::Galaxy && hoveredStar != kInvalidIndex && hoveredStar != selectedStar) {
            const auto &star = stars[hoveredStar];
            Vector2 screen = GetWorldToScreen(star.position, camera);
            DrawTextEx(resources.font, star.data->name.c_str(),
                       Vector2 {screen.x - star.labelWidth * 0.5f, screen.y - 22.0f},
                       16.0f, 1.0f, LIGHTGRAY);
        } else if (mode == ViewMode::System && hoveredPlanet != kInvalidIndex && hoveredPlanet != selectedPlanet) {
            const auto &planet = CurrentSystem().planets[hoveredPlanet];
            Vector2 screen = GetWorldToScreen(planet.position, camera);
            DrawTextEx(resources.font, planet.name.c_str(),
                       Vector2 {screen.x - planet.labelWidth * 0.5f, screen.y - 22.0f},
                       16.0f, 1.0f, LIGHTGRAY);
        }
    }

    void Draw() {
        BeginDrawing();
        ClearBackground(Color {4, 8, 17, 255});
        postProcess.Draw(camera, resources, this, DrawSceneCallback);
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
        postProcess.Unload();
        UnloadRenderResources(resources);
        CloseWindow();
    }
};

Viewer *Viewer::current = nullptr;

int RunViewer(int argc, char *argv[]) {
    int seed = argc > 1 ? std::atoi(argv[1]) : 0;
    int starCount = argc > 2 ? std::atoi(argv[2]) : 64;
    starCount = std::clamp(starCount, 1, 256);
    Viewer viewer(seed, starCount, argc > 3 ? argv[3] : DSP_VIEWER_DEFAULT_FONT);
    if (!viewer.Init()) return 1;
    viewer.Run();
    return 0;
}

}
