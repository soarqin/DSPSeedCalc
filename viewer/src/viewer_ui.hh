#pragma once

#include "view_model.hh"
#include "viewer_layout.hh"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace viewer {

inline constexpr float kBodyListWidth = 248.0f;
inline constexpr float kBodyListMargin = 14.0f;
inline constexpr float kHintBarHeight = 34.0f;

namespace theme {
inline constexpr Color kAccent {88, 198, 255, 255};
inline constexpr Color kAccentSoft {137, 221, 255, 255};
inline constexpr Color kText {236, 243, 249, 255};
inline constexpr Color kTextDim {161, 187, 208, 255};
inline constexpr Color kLabel {123, 157, 183, 255};
inline constexpr Color kHint {98, 131, 159, 255};
inline constexpr Color kPanelFill {7, 15, 28, 236};
inline constexpr Color kPanelBorder {47, 91, 124, 220};
inline constexpr Color kFieldFill {19, 31, 48, 235};
inline constexpr Color kFieldBorder {72, 104, 133, 255};
inline constexpr Color kSeparator {40, 66, 92, 160};
inline constexpr Color kRowHover {34, 66, 96, 150};
inline constexpr Color kRowSelected {32, 101, 151, 190};
inline constexpr Color kWarn {255, 190, 116, 255};
inline constexpr Color kError {255, 118, 108, 255};
}

struct ViewerPanelLayout {
    Rectangle scrollBounds{};
    Rectangle footerBounds{};
    float contentHeight = 0.0f;
    bool scrollRows = false;
};

struct BodyListLayout {
    Rectangle bounds{};
    Rectangle rows{};
    float rowHeight = 24.0f;
    float contentHeight = 0.0f;
};

ViewerPanelLayout GetViewerPanelLayout(Font font, const PanelData &panel, Rectangle bounds,
                                      int footerReserved);
float MeasureTextWidth(Font font, const char *text, int size);
void DrawTextLine(Font font, const char *text, int x, int y, int size, Color color = RAYWHITE);
void DrawHeading(Font font, const std::string &text, int x, int y, int size, float width, Color color);
void DrawTextRight(Font font, const char *text, int right, int y, int size, Color color = RAYWHITE);
void DrawViewerButton(Font font, Rectangle bounds, const char *label, bool accent = false, bool enabled = true);
void DrawPanelBackground(Rectangle bounds);
void DrawViewerPanel(Font font, const PanelData &panel, Rectangle bounds, float scrollOffset,
                     int footerReserved);

BodyListLayout GetBodyListLayout(const ViewerLayout &layout, std::size_t entryCount);
// Returns the entry under `mouse`, or `entryCount` when the point is outside the rows.
std::size_t BodyListHitTest(const BodyListLayout &layout, std::size_t entryCount, float scrollOffset,
                            Vector2 mouse);
void DrawBodyList(Font font, const BodyListLayout &layout, const char *title, const std::vector<BodyListEntry> &entries,
                  float scrollOffset, std::size_t selected, std::size_t hovered);

void DrawHintBar(Font font, const char *text);
void DrawToast(Font font, Rectangle map, const char *text, Color color, float alpha);
void DrawHelpOverlay(Font font, ViewMode mode);
void DrawBusyOverlay(Font font, const char *text);

}
