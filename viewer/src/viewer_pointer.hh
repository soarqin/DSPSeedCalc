#pragma once

#include <raylib.h>

namespace viewer {

struct ViewerPointer {
    Vector2 position{}, delta{};
    bool down = false;
    bool pressed = false;
    bool released = false;

    void Poll();
};

}
