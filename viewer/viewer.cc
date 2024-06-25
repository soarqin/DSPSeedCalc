#include <cfloat>
#include "galaxy.hh"

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
        {0, {1, 0.229, 0.1745}},
        {0.07, {1, 0.4373, 0.1765}},
        {0.32, {1, 0.9515, 0.1557}},
        {0.5, {0.9879, 1, 0.6179}},
        {0.7235, {0.9764, 1, 0.978}},
        {0.82, {0.58, 0.8963, 1}},
        {0.92, {0.1176, 0.6548, 1}},
        {1, {0.0991, 0.34, 1}},
    };

    if (t == 0) {
        color.r = std::roundf(colors[0].color.x * 255.0f);
        color.g = std::roundf(colors[0].color.y * 255.0f);
        color.b = std::roundf(colors[0].color.z * 255.0f);
    }
    color.a = 255;
    for (size_t i = 1; i < 8; i++) {
        auto &gc = colors[i];
        if (t > gc.z) continue;
        auto &lc = colors[i - 1];
        auto lz = lc.z;
        float p = (t - lz) / (gc.z - lz);
        color.r = std::roundf((lc.color.x + (gc.color.x - lc.color.x) * p) * 255.0f);
        color.g = std::roundf((lc.color.y + (gc.color.y - lc.color.y) * p) * 255.0f);
        color.b = std::roundf((lc.color.z + (gc.color.z - lc.color.z) * p) * 255.0f);
        break;
    }
}

void CameraControl(Camera *camera)
{
    bool rotateUp = false;

    // Camera rotation
    if (IsKeyDown(KEY_DOWN)) CameraPitch(camera, -CAMERA_ROTATION_SPEED, false, false, rotateUp);
    if (IsKeyDown(KEY_UP)) CameraPitch(camera, CAMERA_ROTATION_SPEED, false, false, rotateUp);
    if (IsKeyDown(KEY_RIGHT)) CameraYaw(camera, -CAMERA_ROTATION_SPEED, false);
    if (IsKeyDown(KEY_LEFT)) CameraYaw(camera, CAMERA_ROTATION_SPEED, false);
    if (IsKeyDown(KEY_Q)) CameraRoll(camera, -CAMERA_ROTATION_SPEED);
    if (IsKeyDown(KEY_E)) CameraRoll(camera, CAMERA_ROTATION_SPEED);

    // Camera movement
    if (!IsGamepadAvailable(0))
    {
        // Camera pan (for CAMERA_FREE)
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        {
            const Vector2 mouseDelta = GetMouseDelta();
            if (mouseDelta.x > 0.0f) CameraMoveRight(camera, CAMERA_PAN_SPEED, false);
            if (mouseDelta.x < 0.0f) CameraMoveRight(camera, -CAMERA_PAN_SPEED, false);
            if (mouseDelta.y > 0.0f) CameraMoveUp(camera, -CAMERA_PAN_SPEED);
            if (mouseDelta.y < 0.0f) CameraMoveUp(camera, CAMERA_PAN_SPEED);
        }
        else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            // Mouse support
            Vector2 mouseDelta = GetMouseDelta();
            CameraYaw(camera, -mouseDelta.x*CAMERA_MOUSE_MOVE_SENSITIVITY, false);
            CameraPitch(camera, -mouseDelta.y*CAMERA_MOUSE_MOVE_SENSITIVITY, false, false, rotateUp);
        }

        // Keyboard support
        if (IsKeyDown(KEY_W)) CameraMoveForward(camera, CAMERA_MOVE_SPEED, false);
        if (IsKeyDown(KEY_A)) CameraMoveRight(camera, -CAMERA_MOVE_SPEED, false);
        if (IsKeyDown(KEY_S)) CameraMoveForward(camera, -CAMERA_MOVE_SPEED, false);
        if (IsKeyDown(KEY_D)) CameraMoveRight(camera, CAMERA_MOVE_SPEED, false);
    }
    else
    {
        // Gamepad controller support
        CameraYaw(camera, -(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 2)*CAMERA_MOUSE_MOVE_SENSITIVITY, false);
        CameraPitch(camera, -(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 2)*CAMERA_MOUSE_MOVE_SENSITIVITY, false, false, rotateUp);

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

int main(int, char *[]) {
    auto *galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, 0, 64, true, false);
    struct StarData {
        int id;
        Vector3 position;
        float radius;
        Color color;
        const dspugen::Star *data;
    };
    std::vector<StarData> stars;
    for (const auto *s: galaxy->stars) {
        auto &pos = s->position;
        Vector3 p = {float(pos.x), float(pos.y - 10.0f), float(pos.z)};
        Color c;
        switch (s->type) {
            case dspugen::EStarType::WhiteDwarf:
                c = Color {118, 118, 118, 255};
                break;
            case dspugen::EStarType::NeutronStar:
                c = Color {128, 97, 255, 255};
                break;
            case dspugen::EStarType::BlackHole:
                c = Color {0, 0, 0, 255};
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
        stars.emplace_back(StarData{s->id, p, radius * 0.2f, c, s});
    }

    const int screenWidth = 1280;
    const int screenHeight = 720;
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "raylib [core] example - 3d camera free");
    Camera3D camera = {0};
    camera.position = (Vector3){40.0f, -40.0f, 40.0f};
    camera.target = (Vector3){0.0f, -10.0f, 0.0f};
    camera.up = (Vector3){0.0f, -1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    const StarData *selectedStar = nullptr;
    const StarData *collisionStar = nullptr;
    Ray ray = {};
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        CameraControl(&camera);

        if (IsKeyPressed('Z')) camera.target = (Vector3){0.0f, -10.0f, 0.0f};

        ray = GetMouseRay(GetMousePosition(), camera);
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

        BeginDrawing();

        ClearBackground(Color {32, 32, 32, 255});

        BeginMode3D(camera);

        DrawGrid(10, 10.0f);

        for (auto &s: stars) {
            DrawSphere(s.position, s.radius, s.color);
            if (&s == selectedStar) {
                Vector3 dimension = {s.radius * 1.8f, s.radius * 1.8f, s.radius * 1.8f};
                DrawCubeWiresV(s.position, dimension, WHITE);
            } else if (&s == collisionStar) {
                Vector3 dimension = {s.radius * 1.8f, s.radius * 1.8f, s.radius * 1.8f};
                DrawCubeWiresV(s.position, dimension, MAROON);
            }
        }

        EndMode3D();

        if (collisionStar) {
            auto pos2d = GetWorldToScreen(collisionStar->position, camera);
            pos2d.y = pos2d.y - 20.0f;
            const auto *name = collisionStar->data->name.c_str();
            auto w = MeasureText(name, 18);
            DrawText(name, pos2d.x - (w >> 1), pos2d.y, 18, WHITE);
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
