#pragma once

#include "view_model.hh"

#include <raylib.h>

namespace viewer {

class CameraController {
public:
    void InitPlatformInput();
    void Update(Camera3D &camera, ViewMode mode, Vector3 systemTarget, float wheel);
    void ResetGalaxy(Camera3D &camera);
    void ResetSystem(Camera3D &camera, float systemRadius, Vector3 target = {});
    void FocusSystem(Camera3D &camera, Vector3 target, float radius);

private:
    void ApplyGalaxy(Camera3D &camera) const;
    void ApplySystem(Camera3D &camera, Vector3 target) const;

    float galaxyYaw = 0.0f;
    float galaxyPitch = -22.0f;
    float galaxyDistance = 80.9000015f;
    float systemYaw = 0.0f;
    float systemPitch = -32.0f;
    float systemRoll = 0.0f;
    float systemDistance = 40.0f;
};

}
