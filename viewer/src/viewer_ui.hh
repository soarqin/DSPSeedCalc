#pragma once

#include "view_model.hh"

#include <raylib.h>

namespace viewer {

void DrawTextLine(Font font, const char *text, int x, int y, int size, Color color = RAYWHITE);
void DrawViewerButton(Font font, Rectangle bounds, const char *label, int labelWidth, bool accent = false);
void DrawPanelBackground();
void DrawViewerPanel(Font font, const PanelData &panel);

}
