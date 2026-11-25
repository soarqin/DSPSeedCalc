#pragma once

#include <raylib.h>

#include <vector>

namespace viewer {

inline constexpr float kPanelWidth = 340.0f;
inline constexpr float kPanelPadding = 22.0f;
inline constexpr float kTopBarHeight = 64.0f;

enum class CompactPage { Map, List, Details };

struct ViewerLayout {
    bool compact;
    Rectangle topBar, map, panel, navigation;
    Rectangle seed, starCount, apply, random;

    Rectangle Tab(CompactPage page) const;
};

ViewerLayout CalculateViewerLayout(int width, int height);

// Try the game's preferred offset first, then the other sides of the same body.
bool PlaceSceneLabel(Rectangle &label, Vector2 body, Rectangle map,
                     const std::vector<Rectangle> &occupied);

}
