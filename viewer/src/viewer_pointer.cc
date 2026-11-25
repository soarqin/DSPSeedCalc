#include "viewer_pointer.hh"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <deque>
#endif

namespace viewer {

#if defined(__EMSCRIPTEN__)
namespace {
enum class PointerEvent { Press, Move, Release, Cancel };
struct Sample {
    PointerEvent event;
    Vector2 point;
};
std::deque<Sample> samples;
}

extern "C" EMSCRIPTEN_KEEPALIVE void viewerPointerEvent(int type, float x, float y) {
    Sample sample{static_cast<PointerEvent>(type), {x * GetScreenWidth(), y * GetScreenHeight()}};
    if (sample.event == PointerEvent::Move && !samples.empty() && samples.back().event == PointerEvent::Move)
        samples.back() = sample;
    else
        samples.push_back(sample);
}
#endif

void ViewerPointer::Poll() {
#if defined(__EMSCRIPTEN__)
    // Preserve both edges of a tap even when they arrive between animation frames.
    pressed = released = false;
    Vector2 previous = position;
    if (!samples.empty()) {
        auto sample = samples.front();
        samples.pop_front();
        position = sample.point;
        pressed = sample.event == PointerEvent::Press;
        released = sample.event == PointerEvent::Release;
        down = pressed || (down && sample.event == PointerEvent::Move);
    } else if (!down) {
        position = GetMousePosition();
    }
    delta = pressed ? Vector2{} : Vector2{position.x - previous.x, position.y - previous.y};
#else
    position = GetMousePosition();
    delta = GetMouseDelta();
    pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
#endif
}

}
