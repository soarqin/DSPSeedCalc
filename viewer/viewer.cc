#include <cfloat>
#include "galaxy.hh"
#include "protoset.hh"
#include "settings.hh"

#include <raylib.h>
#include <rcamera.h>
#include <raymath.h>

#define CAMERA_MOVE_SPEED                               0.09f
#define CAMERA_ROTATION_SPEED                           0.03f
#define CAMERA_PAN_SPEED                                0.2f
#define CAMERA_MOUSE_MOVE_SENSITIVITY                   0.003f

void GradiantColor(float t, Color &color) {
    const struct {
        float z;

        struct {
            float x, y, z;
        } color;
    } colors[] = {
            { 0, { 1, 0.229, 0.1745 } },
            { 0.07, { 1, 0.4373, 0.1765 } },
            { 0.32, { 1, 0.9515, 0.1557 } },
            { 0.5, { 0.9879, 1, 0.6179 } },
            { 0.7235, { 0.9764, 1, 0.978 } },
            { 0.82, { 0.58, 0.8963, 1 } },
            { 0.92, { 0.1176, 0.6548, 1 } },
            { 1, { 0.0991, 0.34, 1 } },
        };

    if (t == 0) {
        color.r = static_cast<uint8_t>(std::roundf(colors[0].color.x * 255.0f));
        color.g = static_cast<uint8_t>(std::roundf(colors[0].color.y * 255.0f));
        color.b = static_cast<uint8_t>(std::roundf(colors[0].color.z * 255.0f));
    }
    color.a = 255;
    for (size_t i = 1; i < 8; i++) {
        auto &gc = colors[i];
        if (t > gc.z) continue;
        auto &lc = colors[i - 1];
        auto lz = lc.z;
        float p = (t - lz) / (gc.z - lz);
        color.r = static_cast<uint8_t>(std::roundf((lc.color.x + (gc.color.x - lc.color.x) * p) * 255.0f));
        color.g = static_cast<uint8_t>(std::roundf((lc.color.y + (gc.color.y - lc.color.y) * p) * 255.0f));
        color.b = static_cast<uint8_t>(std::roundf((lc.color.z + (gc.color.z - lc.color.z) * p) * 255.0f));
        break;
    }
}

void CameraControl(Camera *camera) {
    bool rotateUp = false;

    // Camera rotation
    if (IsKeyDown(KEY_DOWN)) CameraPitch(camera, -CAMERA_ROTATION_SPEED, false, false, rotateUp);
    if (IsKeyDown(KEY_UP)) CameraPitch(camera, CAMERA_ROTATION_SPEED, false, false, rotateUp);
    if (IsKeyDown(KEY_RIGHT)) CameraYaw(camera, -CAMERA_ROTATION_SPEED, false);
    if (IsKeyDown(KEY_LEFT)) CameraYaw(camera, CAMERA_ROTATION_SPEED, false);
    if (IsKeyDown(KEY_Q)) CameraRoll(camera, -CAMERA_ROTATION_SPEED);
    if (IsKeyDown(KEY_E)) CameraRoll(camera, CAMERA_ROTATION_SPEED);

    // Camera movement
    if (!IsGamepadAvailable(0)) {
        // Camera pan (for CAMERA_FREE)
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            const Vector2 mouseDelta = GetMouseDelta();
            if (mouseDelta.x > 0.0f) CameraMoveRight(camera, CAMERA_PAN_SPEED, false);
            if (mouseDelta.x < 0.0f) CameraMoveRight(camera, -CAMERA_PAN_SPEED, false);
            if (mouseDelta.y > 0.0f) CameraMoveUp(camera, -CAMERA_PAN_SPEED);
            if (mouseDelta.y < 0.0f) CameraMoveUp(camera, CAMERA_PAN_SPEED);
        } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            // Mouse support
            Vector2 mouseDelta = GetMouseDelta();
            CameraYaw(camera, -mouseDelta.x * CAMERA_MOUSE_MOVE_SENSITIVITY, false);
            CameraPitch(camera, -mouseDelta.y * CAMERA_MOUSE_MOVE_SENSITIVITY, false, false, rotateUp);
        }

        // Keyboard support
        if (IsKeyDown(KEY_W)) CameraMoveForward(camera, CAMERA_MOVE_SPEED, false);
        if (IsKeyDown(KEY_A)) CameraMoveRight(camera, -CAMERA_MOVE_SPEED, false);
        if (IsKeyDown(KEY_S)) CameraMoveForward(camera, -CAMERA_MOVE_SPEED, false);
        if (IsKeyDown(KEY_D)) CameraMoveRight(camera, CAMERA_MOVE_SPEED, false);
    } else {
        // Gamepad controller support
        CameraYaw(camera, -(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 2) * CAMERA_MOUSE_MOVE_SENSITIVITY, false);
        CameraPitch(camera, -(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 2) * CAMERA_MOUSE_MOVE_SENSITIVITY, false, false, rotateUp);

        if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) <= -0.25f) CameraMoveForward(camera, CAMERA_MOVE_SPEED, false);
        if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) <= -0.25f) CameraMoveRight(camera, -CAMERA_MOVE_SPEED, false);
        if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) >= 0.25f) CameraMoveForward(camera, -CAMERA_MOVE_SPEED, false);
        if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) >= 0.25f) CameraMoveRight(camera, CAMERA_MOVE_SPEED, false);
    }

    if (IsKeyDown(KEY_SPACE)) CameraMoveUp(camera, CAMERA_MOVE_SPEED);
    if (IsKeyDown(KEY_LEFT_CONTROL)) CameraMoveUp(camera, -CAMERA_MOVE_SPEED);

    // Zoom target distance
    CameraMoveToTarget(camera, -GetMouseWheelMove());
    if (IsKeyPressed(KEY_KP_SUBTRACT)) CameraMoveToTarget(camera, 2.0f);
    if (IsKeyPressed(KEY_KP_ADD)) CameraMoveToTarget(camera, -2.0f);
}

static Vector2 storedMousePosition;

void StoreCursorPosition() {
    storedMousePosition = GetMousePosition();
}

void RestoreCursorPosition() {
    SetMousePosition(static_cast<int>(storedMousePosition.x), static_cast<int>(storedMousePosition.y));
}

int main(int, char *[]) {
    constexpr int screenWidth = 1280;
    constexpr int screenHeight = 720;
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "Dyson Sphere Program Universe Viewer");

    int codepoints[95 + 11];
    for (int i = 0; i < 95; i++) {
        codepoints[i] = 32 + i;
    }
    for (int i = 0; i < 11; i++) {
        codepoints[95 + i] = 0x3B1 + i;
    }
    auto font = LoadFontEx("Roboto-Regular.ttf", 18, codepoints, 95 + 11);

    dspugen::settings.hasPlanets = true;
    dspugen::settings.birthOnly = false;
    dspugen::settings.genName = true;
    dspugen::loadProtoSets();

    dspugen::Galaxy::initThread();
    dspugen::Star::initThread();
    dspugen::Planet::initThread();
    auto *galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, 0, 64);
    struct StarData {
        int id;
        Vector3 position;
        float radius;
        Color color;
        float nameWidth;
        const dspugen::Star *data;
    };
    std::vector<StarData> stars;
    for (const auto *s: galaxy->stars) {
        auto &pos = s->position;
        Vector3 p = { static_cast<float>(pos.x), static_cast<float>(pos.y - 10.0f), static_cast<float>(pos.z) };
        Color c;
        switch (s->type) {
            case dspugen::EStarType::WhiteDwarf:
                c = Color { 118, 118, 118, 255 };
                break;
            case dspugen::EStarType::NeutronStar:
                c = Color { 128, 97, 255, 255 };
                break;
            case dspugen::EStarType::BlackHole:
                c = Color { 0, 0, 0, 255 };
                break;
            default:
                GradiantColor(s->color, c);
        }
        float radius;
        switch (s->type) {
            case dspugen::EStarType::GiantStar:
                radius = 4.0f;
                break;
            case dspugen::EStarType::WhiteDwarf:
            case dspugen::EStarType::NeutronStar:
                radius = 0.8f;
                break;
            case dspugen::EStarType::BlackHole:
                radius = 1.0f;
                break;
            default:
                radius = 1.5f;
        }
        auto v2 = MeasureTextEx(font, s->name.c_str(), 18, 0);
        stars.emplace_back(StarData { s->id, p, radius * 0.2f, c, v2.x, s });
    }

    Camera3D camera = { 0 };
    camera.position = (Vector3) { 40.0f, -40.0f, 40.0f };
    camera.target = (Vector3) { 0.0f, -10.0f, 0.0f };
    camera.up = (Vector3) { 0.0f, -1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    const StarData *selectedStar = nullptr;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        auto needHide = IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) || IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
        if (IsCursorHidden() != needHide) {
            needHide ? (StoreCursorPosition(), DisableCursor()) : (EnableCursor(), RestoreCursorPosition());
        }
        CameraControl(&camera);

        if (IsKeyPressed('Z')) camera.target = (Vector3) { 0.0f, -10.0f, 0.0f };

        const StarData *collisionStar;
        if (IsCursorHidden()) {
            collisionStar = nullptr;
        } else {
            auto ray = GetMouseRay(GetMousePosition(), camera);
            float collisionDistance = FLT_MAX;
            collisionStar = nullptr;
            for (auto &s: stars) {
                auto collision = GetRayCollisionSphere(ray, s.position, s.radius);
                if (collision.hit && collision.distance >= 0.5) {
                    if (collision.distance < collisionDistance) {
                        collisionStar = &s;
                        collisionDistance = collision.distance;
                    }
                }
            }

            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                selectedStar = collisionStar;
            }
        }

        BeginDrawing();

        ClearBackground(Color { 32, 32, 32, 255 });

        BeginMode3D(camera);

        DrawGrid(8, 10.0f);

        for (auto &s: stars) {
            DrawSphere(s.position, s.radius, s.color);
            if (&s == selectedStar) {
                Vector3 dimension = { s.radius * 1.8f, s.radius * 1.8f, s.radius * 1.8f };
                DrawCubeWiresV(s.position, dimension, WHITE);
            } else if (&s == collisionStar) {
                Vector3 dimension = { s.radius * 1.8f, s.radius * 1.8f, s.radius * 1.8f };
                DrawCubeWiresV(s.position, dimension, LIGHTGRAY);
            }
        }

        EndMode3D();

        if (IsKeyDown(KEY_LEFT_ALT)) {
            for (auto &s: stars) {
                auto pos2d = GetWorldToScreen(s.position, camera);
                pos2d.y = pos2d.y - 20.0f;
                pos2d.x -= s.nameWidth * 0.5f;
                DrawTextEx(font, s.data->name.c_str(), pos2d, 18, 0, WHITE);
            }
        } else {
            if (selectedStar) {
                auto pos2d = GetWorldToScreen(selectedStar->position, camera);
                pos2d.y = pos2d.y - 20.0f;
                pos2d.x -= selectedStar->nameWidth * 0.5f;
                DrawTextEx(font, selectedStar->data->name.c_str(), pos2d, 18, 0, WHITE);
            }

            if (collisionStar && collisionStar != selectedStar) {
                auto pos2d = GetWorldToScreen(collisionStar->position, camera);
                pos2d.y = pos2d.y - 20.0f;
                pos2d.x -= collisionStar->nameWidth * 0.5f;
                DrawTextEx(font, collisionStar->data->name.c_str(), pos2d, 18, 0, LIGHTGRAY);
            }
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
