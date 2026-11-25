#include "camera_controller.hh"

#include <raymath.h>

#include <algorithm>
#include <cmath>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace viewer {
namespace {

constexpr float kCameraRotationSpeed = 0.03f;
constexpr float kCameraMouseSensitivity = 0.003f;
constexpr float kGalaxyCameraDistance = 80.9000015f;
constexpr float kGalaxyCameraZoomScale = 1.25f;
constexpr float kGalaxyCameraDistanceMin = 45.0f;
constexpr float kGalaxyCameraDistanceMax = 160.0f;
constexpr float kGalaxyCameraTargetHeight = 11.0f;
constexpr float kGalaxyCameraPitchBegin = -22.0f;
constexpr float kGalaxyCameraPitchMin = -81.1999969f;
constexpr float kGalaxyCameraPitchMax = -4.9000001f;
constexpr float kSystemCameraPitchBegin = -32.0f;
constexpr float kSystemCameraPitchMin = -89.0f;
constexpr float kSystemCameraPitchMax = 89.0f;
constexpr float kSystemCameraZoomScale = 1.2f;
constexpr float kSystemCameraDistanceMin = 0.0375f;
constexpr float kSystemCameraDistanceMax = 45000.0f;
/* Exponential approach rate of the zoom distance, per second. */
constexpr float kZoomSmoothing = 14.0f;
constexpr float kTransitionSeconds = 0.6f;

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
    webMouse = {};
    return EM_FALSE;
}

EM_BOOL HandleWebFocus(int eventType, const EmscriptenFocusEvent *, void *) {
    if (eventType == EMSCRIPTEN_EVENT_BLUR) webMouse = {};
    return EM_FALSE;
}
#endif

float SmoothDistance(float current, float wanted, float dt) {
    float k = 1.0f - std::exp(-kZoomSmoothing * dt);
    float logCurrent = std::log(current);
    float next = std::exp(logCurrent + (std::log(wanted) - logCurrent) * k);
    return std::abs(next - wanted) <= wanted * 0.0005f ? wanted : next;
}

float MapVerticalFov() {
    int width = std::max(1, GetScreenWidth());
    int height = GetScreenHeight();
    if (width >= height) return 60.0f;
    // In portrait keep the same horizontal field of view so the cluster fits across the short side.
    return 2.0f * std::atan(std::tan(30.0f * DEG2RAD) * static_cast<float>(height) / width) * RAD2DEG;
}

float LerpAngle(float from, float to, float t) {
    float delta = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
    return from + delta * t;
}

float EaseInOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}

void CameraController::InitPlatformInput() {
#if defined(__EMSCRIPTEN__)
    emscripten_set_mousedown_callback("#canvas", nullptr, true, HandleWebMouse);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, HandleWebMouse);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, HandleWebMouse);
    emscripten_set_mouseleave_callback("#canvas", nullptr, true, HandleWebMouseLeave);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, HandleWebFocus);
#endif
}

void CameraController::ApplyGalaxy(Camera3D &camera) const {
    float yawRadians = galaxyYaw * DEG2RAD;
    float pitchRadians = galaxyPitch * DEG2RAD;
    float sinYaw = std::sin(yawRadians);
    float cosYaw = std::cos(yawRadians);
    float sinPitch = std::sin(pitchRadians);
    float cosPitch = std::cos(pitchRadians);
    Vector3 forward {sinYaw * cosPitch, sinPitch, -cosYaw * cosPitch};
    camera.target = Vector3 {0.0f, kGalaxyCameraTargetHeight, 0.0f};
    camera.position = camera.target - forward * galaxyDistance;
    camera.up = Vector3 {-sinYaw * sinPitch, cosPitch, cosYaw * sinPitch};
}

void CameraController::ApplySystem(Camera3D &camera, Vector3 target) const {
    float yawRadians = systemYaw * DEG2RAD;
    float pitchRadians = systemPitch * DEG2RAD;
    float sinYaw = std::sin(yawRadians);
    float cosYaw = std::cos(yawRadians);
    float sinPitch = std::sin(pitchRadians);
    float cosPitch = std::cos(pitchRadians);
    Vector3 forward {sinYaw * cosPitch, sinPitch, -cosYaw * cosPitch};
    Vector3 up {-sinYaw * sinPitch, cosPitch, cosYaw * sinPitch};
    Quaternion rollRotation = QuaternionFromAxisAngle(forward, systemRoll * DEG2RAD);
    camera.target = target;
    camera.position = target - forward * systemDistance;
    camera.up = Vector3Normalize(Vector3RotateByQuaternion(up, rollRotation));
}

void CameraController::StartTransition(bool angles, float goalYaw, float goalPitch, float goalRoll) {
    transition.active = true;
    transition.angles = angles;
    transition.progress = 0.0f;
    transition.goalYaw = goalYaw;
    transition.goalPitch = goalPitch;
    transition.goalRoll = goalRoll;
    if (transitionMode == ViewMode::Galaxy) {
        transition.yaw = galaxyYaw;
        transition.pitch = galaxyPitch;
        transition.roll = 0.0f;
    } else {
        transition.yaw = systemYaw;
        transition.pitch = systemPitch;
        transition.roll = systemRoll;
        transition.target = systemTargetCurrent;
    }
}

void CameraController::AdvanceTransition(float dt, bool userRotated) {
    if (!transition.active) return;
    if (userRotated) transition.angles = false;
    transition.progress = std::min(1.0f, transition.progress + dt / kTransitionSeconds);
    float eased = EaseInOut(transition.progress);
    if (transition.angles) {
        float yaw = LerpAngle(transition.yaw, transition.goalYaw, eased);
        float pitch = transition.pitch + (transition.goalPitch - transition.pitch) * eased;
        if (transitionMode == ViewMode::Galaxy) {
            galaxyYaw = yaw;
            galaxyPitch = pitch;
        } else {
            systemYaw = yaw;
            systemPitch = pitch;
            systemRoll = LerpAngle(transition.roll, transition.goalRoll, eased);
        }
    }
    if (transition.progress >= 1.0f) transition.active = false;
}

void CameraController::ResetGalaxy(Camera3D &camera, bool animate) {
    transitionMode = ViewMode::Galaxy;
    galaxyDistanceWanted = kGalaxyCameraDistance;
    camera.fovy = MapVerticalFov();
    camera.projection = CAMERA_PERSPECTIVE;
    if (animate) {
        StartTransition(true, 0.0f, kGalaxyCameraPitchBegin, 0.0f);
        return;
    }
    transition.active = false;
    galaxyYaw = 0.0f;
    galaxyPitch = kGalaxyCameraPitchBegin;
    galaxyDistance = kGalaxyCameraDistance;
    ApplyGalaxy(camera);
}

void CameraController::ResetSystem(Camera3D &camera, float systemRadius, Vector3 target, bool animate) {
    transitionMode = ViewMode::System;
    systemDistanceWanted = std::clamp(systemRadius * 1.35f, kSystemCameraDistanceMin, kSystemCameraDistanceMax);
    camera.fovy = MapVerticalFov();
    camera.projection = CAMERA_PERSPECTIVE;
    if (animate) {
        StartTransition(true, 0.0f, kSystemCameraPitchBegin, 0.0f);
        return;
    }
    transition.active = false;
    systemYaw = 0.0f;
    systemPitch = kSystemCameraPitchBegin;
    systemRoll = 0.0f;
    systemDistance = systemDistanceWanted;
    systemTargetCurrent = target;
    ApplySystem(camera, target);
}

void CameraController::FocusSystem(Camera3D &camera, Vector3 target, float radius) {
    transitionMode = ViewMode::System;
    systemDistanceWanted = std::clamp(radius * 4.5f, kSystemCameraDistanceMin, kSystemCameraDistanceMax);
    StartTransition(false, systemYaw, systemPitch, systemRoll);
    (void)camera;
    (void)target;
}

void CameraController::ScaleDistance(ViewMode mode, float factor) {
    if (mode == ViewMode::Galaxy)
        galaxyDistanceWanted = std::clamp(galaxyDistanceWanted * factor, kGalaxyCameraDistanceMin, kGalaxyCameraDistanceMax);
    else
        systemDistanceWanted = std::clamp(systemDistanceWanted * factor, kSystemCameraDistanceMin, kSystemCameraDistanceMax);
}

void CameraController::Update(Camera3D &camera, ViewMode mode, Vector3 systemTarget, float wheel,
                              bool primaryOrbit, Vector2 primaryDelta, bool overMap) {
    camera.fovy = MapVerticalFov();
    float dt = std::clamp(GetFrameTime(), 0.0f, 0.1f);
    bool middleDown = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    bool rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    Vector2 delta = GetMouseDelta();
#if defined(__EMSCRIPTEN__)
    middleDown = webMouse.middleDown;
    rightDown = webMouse.rightDown;
    if (middleDown || rightDown) delta = webMouse.delta;
    webMouse.delta = {};
#endif
    bool secondary = middleDown || rightDown;
    if (secondary && !secondaryDown) secondaryOnMap = overMap;
    secondaryDown = secondary;
    bool orbitDrag = (middleDown && secondaryOnMap) || primaryOrbit;
    bool rollDrag = rightDown && secondaryOnMap && mode == ViewMode::System;
    if (primaryOrbit) delta = primaryDelta;
    if (orbitDrag || rollDrag) wheel = 0.0f;

    float keyStep = kCameraRotationSpeed * RAD2DEG * 60.0f * dt;
    float yawStep = (IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT)) * keyStep;
    float pitchStep = (IsKeyDown(KEY_UP) - IsKeyDown(KEY_DOWN)) * keyStep;
    if (IsGamepadAvailable(0)) {
        float stickStep = 2.0f * kCameraMouseSensitivity * RAD2DEG * 60.0f * dt;
        float x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        float y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
        if (std::abs(x) > 0.12f) yawStep += x * stickStep;
        if (std::abs(y) > 0.12f) pitchStep -= y * stickStep;
    }
    if (orbitDrag) {
        yawStep += delta.x * kCameraMouseSensitivity * RAD2DEG;
        pitchStep -= delta.y * kCameraMouseSensitivity * RAD2DEG;
    }
    if (transitionMode != mode) transition.active = false;
    transitionMode = mode;
    AdvanceTransition(dt, yawStep != 0 || pitchStep != 0 || rollDrag);

    if (mode == ViewMode::Galaxy) {
        galaxyYaw += yawStep;
        galaxyPitch += pitchStep;
        if (wheel != 0) ScaleDistance(mode, std::pow(kGalaxyCameraZoomScale, -wheel));
        galaxyDistance = SmoothDistance(galaxyDistance, galaxyDistanceWanted, dt);
        galaxyYaw = std::fmod(galaxyYaw, 360.0f);
        if (galaxyYaw < 0.0f) galaxyYaw += 360.0f;
        galaxyPitch = std::clamp(galaxyPitch, kGalaxyCameraPitchMin, kGalaxyCameraPitchMax);
        ApplyGalaxy(camera);
        return;
    }

    systemYaw += yawStep;
    systemPitch += pitchStep * 0.5f;
    if (rollDrag && !orbitDrag) systemRoll -= delta.x * kCameraMouseSensitivity * 0.7f * RAD2DEG;
    if (wheel != 0) ScaleDistance(mode, std::pow(kSystemCameraZoomScale, -wheel));
    systemDistance = SmoothDistance(systemDistance, systemDistanceWanted, dt);
    systemYaw = std::fmod(systemYaw, 360.0f);
    if (systemYaw < 0.0f) systemYaw += 360.0f;
    systemPitch = std::clamp(systemPitch, kSystemCameraPitchMin, kSystemCameraPitchMax);
    /* The target eases from where the transition started toward the live goal, so a moving planet is
       caught exactly when the transition ends instead of trailing behind it. */
    if (transition.active)
        systemTargetCurrent = Vector3Lerp(transition.target, systemTarget, EaseInOut(transition.progress));
    else
        systemTargetCurrent = systemTarget;
    ApplySystem(camera, systemTargetCurrent);
}

}
