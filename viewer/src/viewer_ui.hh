#pragma once

#include "view_model.hh"

#include <raylib.h>

namespace viewer {

struct ViewerPanelLayout {
    Rectangle mineralBounds{};
    Rectangle footerBounds{};
    float mineralContentHeight = 0.0f;
};

ViewerPanelLayout GetViewerPanelLayout(Font font, const PanelData &panel,
                                       int footerReserved = 170);
void DrawTextLine(Font font, const char *text, int x, int y, int size, Color color = RAYWHITE);
void DrawViewerButton(Font font, Rectangle bounds, const char *label, int labelWidth, bool accent = false);
void DrawPanelBackground();
void DrawViewerPanel(Font font, const PanelData &panel, float scrollOffset = 0.0f,
                     int footerReserved = 170);

}
