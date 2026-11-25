#include "viewer_layout.hh"

#include <cassert>

int main() {
    for (int width : {320, 390, 844, 959, 960, 1024, 1280, 1920}) {
        auto layout = viewer::CalculateViewerLayout(width, 680);
        assert(layout.map.width > 0 && layout.map.height > 0);
        float previousEnd = 0;
        for (auto control : {layout.seed, layout.starCount, layout.apply, layout.random}) {
            assert(control.x >= previousEnd && control.width > 0);
            previousEnd = control.x + control.width;
        }
        assert(previousEnd <= layout.topBar.width);
        if (layout.compact) {
            assert(layout.panel.width == width && layout.map.width == width);
            auto last = layout.Tab(viewer::CompactPage::Details);
            assert(last.x + last.width >= width - 0.01f);
            assert(last.y >= layout.panel.y + layout.panel.height);
        } else {
            assert(layout.map.width == layout.panel.x);
        }
    }

    Rectangle map{0, 64, 320, 380};
    Vector2 body{160, 220};
    Rectangle preferred{170, 214, 90, 16};
    auto first = preferred;
    assert(viewer::PlaceSceneLabel(first, body, map, {}));
    assert(first.x == preferred.x && first.y == preferred.y);
    auto second = preferred;
    assert(viewer::PlaceSceneLabel(second, body, map, {first}));
    assert(!CheckCollisionRecs(first, second));
    assert(!viewer::PlaceSceneLabel(second, body, map, {map}));
    assert(!viewer::PlaceSceneLabel(second, {160, 0}, map, {}));
    Rectangle edge{304, 214, 90, 16};
    assert(viewer::PlaceSceneLabel(edge, {296, 220}, map, {}));
    assert(edge.x + edge.width < 296);
    Rectangle closeUp{350, 214, 150, 16};
    Rectangle marker{40, 100, 240, 240};
    assert(viewer::PlaceSceneLabel(closeUp, body, map, {marker}));
    assert(!CheckCollisionRecs(closeUp, marker));
}
