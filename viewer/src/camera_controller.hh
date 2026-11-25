#pragma once

#include "view_model.hh"

#include <raylib.h>

namespace viewer {

class CameraController {
public:
    void InitPlatformInput();
    /* `primaryOrbit` is true while a left-button drag should orbit the camera like the middle button. */
    void Update(Camera3D &camera, ViewMode mode, Vector3 systemTarget, float wheel,
                bool primaryOrbit, Vector2 primaryDelta, bool overMap);
    void ResetGalaxy(Camera3D &camera, bool animate = false);
    void ScaleDistance(ViewMode mode, float factor);
    void ResetSystem(Camera3D &camera, float systemRadius, Vector3 target = {}, bool animate = false);
    void FocusSystem(Camera3D &camera, Vector3 target, float radius);

private:
    struct Transition {
        bool active = false;
        bool angles = false;
        float progress = 0.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
        float goalYaw = 0.0f;
        float goalPitch = 0.0f;
        float goalRoll = 0.0f;
        Vector3 target{};
    };

    void ApplyGalaxy(Camera3D &camera) const;
    void ApplySystem(Camera3D &camera, Vector3 target) const;
    void StartTransition(bool angles, float goalYaw, float goalPitch, float goalRoll);
    void AdvanceTransition(float dt, bool userRotated);

    float galaxyYaw = 0.0f;
    float galaxyPitch = -22.0f;
    float galaxyDistance = 80.9000015f;
    float galaxyDistanceWanted = 80.9000015f;
    float systemYaw = 0.0f;
    float systemPitch = -32.0f;
    float systemRoll = 0.0f;
    float systemDistance = 40.0f;
    float systemDistanceWanted = 40.0f;
    Vector3 systemTargetCurrent{};
    ViewMode transitionMode = ViewMode::Galaxy;
    Transition transition;
    bool secondaryDown = false;
    bool secondaryOnMap = false;
};

}
