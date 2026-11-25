#include "viewer_layout.hh"

#include <algorithm>

namespace viewer {

Rectangle ViewerLayout::Tab(CompactPage page) const {
    float width = navigation.width / 3.0f;
    return {static_cast<int>(page) * width, navigation.y, width, navigation.height};
}

ViewerLayout CalculateViewerLayout(int width, int height) {
    float w = static_cast<float>(width);
    float h = static_cast<float>(height);
    ViewerLayout layout{};
    layout.compact = w < 960.0f;
    float mapWidth = layout.compact ? w : w - kPanelWidth;
    float top = std::min(kTopBarHeight, h);
    float bottom = layout.compact ? std::min(48.0f, h - top) : 0.0f;
    layout.topBar = {0, 0, mapWidth, top};
    layout.map = {0, top, mapWidth, h - top - bottom};
    layout.navigation = {0, h - bottom, mapWidth, bottom};
    layout.panel = layout.compact ? layout.map : Rectangle{mapWidth, 0, kPanelWidth, h};

    float margin = mapWidth < 600.0f ? 8.0f : 20.0f;
    float controls = std::min(370.0f, mapWidth - 2.0f * margin);
    float x = mapWidth - margin - controls;
    float seedWidth = std::max(1.0f, controls - 206.0f);
    layout.seed = {x, 20, seedWidth, 38};
    layout.starCount = {x + seedWidth + 6, 20, 48, 38};
    layout.apply = {layout.starCount.x + 54, 20, 60, 38};
    layout.random = {layout.apply.x + 66, 20, 80, 38};
    return layout;
}

bool PlaceSceneLabel(Rectangle &label, Vector2 body, Rectangle map,
                     const std::vector<Rectangle> &occupied) {
    if (!CheckCollisionPointRec(body, map) || label.width + 4 > map.width) return false;
    float right = label.x;
    float left = body.x - (right - body.x) - label.width;
    float above = body.y - label.height - 6.0f;
    float below = body.y + 6.0f;
    for (Rectangle bounds : occupied) {
        if (!CheckCollisionPointRec(body, bounds)) continue;
        above = std::min(above, bounds.y - label.height - 6);
        below = std::max(below, bounds.y + bounds.height + 6);
    }
    float center = std::clamp(body.x - label.width * 0.5f, map.x + 2, map.x + map.width - label.width - 2);
    for (Vector2 pos : {Vector2{right, label.y}, {left, label.y}, {center, above}, {center, below},
                        {right, above}, {left, above}, {right, below}, {left, below}}) {
        Rectangle candidate{pos.x - 2, pos.y - 2, label.width + 4, label.height + 4};
        if (candidate.x < map.x || candidate.y < map.y ||
            candidate.x + candidate.width > map.x + map.width ||
            candidate.y + candidate.height > map.y + map.height) continue;
        if (std::any_of(occupied.begin(), occupied.end(), [&](Rectangle other) {
                return CheckCollisionRecs(candidate, other);
            })) continue;
        label.x = pos.x;
        label.y = pos.y;
        return true;
    }
    return false;
}

}
