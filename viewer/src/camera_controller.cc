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

void CameraController::ResetGalaxy(Camera3D &camera) {
    galaxyYaw = 0.0f;
    galaxyPitch = kGalaxyCameraPitchBegin;
    galaxyDistance = kGalaxyCameraDistance;
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    ApplyGalaxy(camera);
}

void CameraController::ResetSystem(Camera3D &camera, float systemRadius, Vector3 target) {
    systemYaw = 0.0f;
    systemPitch = kSystemCameraPitchBegin;
    systemRoll = 0.0f;
    systemDistance = systemRadius * 1.35f;
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    ApplySystem(camera, target);
}

void CameraController::FocusSystem(Camera3D &camera, Vector3 target, float radius) {
    systemDistance = std::clamp(radius * 4.5f, kSystemCameraDistanceMin, kSystemCameraDistanceMax);
    ApplySystem(camera, target);
}

void CameraController::Update(Camera3D &camera, ViewMode mode, Vector3 systemTarget, float wheel) {
    bool middleDown = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    bool rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    Vector2 delta = GetMouseDelta();
#if defined(__EMSCRIPTEN__)
    middleDown = webMouse.middleDown;
    rightDown = webMouse.rightDown;
    delta = webMouse.delta;
    webMouse.delta = {};
#endif
    if (middleDown || rightDown) wheel = 0.0f;

    if (mode == ViewMode::Galaxy) {
        if (IsKeyDown(KEY_DOWN)) galaxyPitch -= kCameraRotationSpeed * RAD2DEG;
        if (IsKeyDown(KEY_UP)) galaxyPitch += kCameraRotationSpeed * RAD2DEG;
        if (IsKeyDown(KEY_RIGHT)) galaxyYaw += kCameraRotationSpeed * RAD2DEG;
        if (IsKeyDown(KEY_LEFT)) galaxyYaw -= kCameraRotationSpeed * RAD2DEG;
        if (!IsGamepadAvailable(0)) {
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
        if (std::abs(wheel) > 0.0f) galaxyDistance *= std::pow(kGalaxyCameraZoomScale, -wheel);
        galaxyDistance = std::clamp(galaxyDistance, kGalaxyCameraDistanceMin, kGalaxyCameraDistanceMax);
        galaxyYaw = std::fmod(galaxyYaw, 360.0f);
        if (galaxyYaw < 0.0f) galaxyYaw += 360.0f;
        galaxyPitch = std::clamp(galaxyPitch, kGalaxyCameraPitchMin, kGalaxyCameraPitchMax);
        ApplyGalaxy(camera);
        return;
    }

    if (!IsGamepadAvailable(0)) {
        if (middleDown) {
            systemYaw += delta.x * kCameraMouseSensitivity * RAD2DEG;
            systemPitch -= delta.y * kCameraMouseSensitivity * 0.5f * RAD2DEG;
        } else if (rightDown) {
            systemRoll -= delta.x * kCameraMouseSensitivity * 0.7f * RAD2DEG;
        }
    } else {
        systemYaw += GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) *
                     2.0f * kCameraMouseSensitivity * RAD2DEG;
        systemPitch -= GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) *
                       2.0f * kCameraMouseSensitivity * 0.5f * RAD2DEG;
    }
    if (std::abs(wheel) > 0.0f) systemDistance *= std::pow(kSystemCameraZoomScale, -wheel);
    systemDistance = std::clamp(systemDistance, kSystemCameraDistanceMin, kSystemCameraDistanceMax);
    systemYaw = std::fmod(systemYaw, 360.0f);
    if (systemYaw < 0.0f) systemYaw += 360.0f;
    systemPitch = std::clamp(systemPitch, kSystemCameraPitchMin, kSystemCameraPitchMax);
    ApplySystem(camera, systemTarget);
}

}
