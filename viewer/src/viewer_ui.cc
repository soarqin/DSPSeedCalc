#include "viewer_ui.hh"

#include <algorithm>

namespace viewer {
namespace {

constexpr float kPanelWidth = 326.0f;

Color AddColor(Color color, int amount) {
    color.r = static_cast<unsigned char>(std::clamp(static_cast<int>(color.r) + amount, 0, 255));
    color.g = static_cast<unsigned char>(std::clamp(static_cast<int>(color.g) + amount, 0, 255));
    color.b = static_cast<unsigned char>(std::clamp(static_cast<int>(color.b) + amount, 0, 255));
    return color;
}

int DrawKeyValue(Font font, const PanelRow &row, int y) {
    int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
    DrawTextLine(font, row.key, x, y, 15, Color {123, 157, 183, 255});
    DrawTextLine(font, row.value.c_str(), x + 112, y, 15, RAYWHITE);
    return y + 25;
}

}

void DrawTextLine(Font font, const char *text, int x, int y, int size, Color color) {
    DrawTextEx(font, text, Vector2 {static_cast<float>(x), static_cast<float>(y)},
               static_cast<float>(size), 1.0f, color);
}

void DrawViewerButton(Font font, Rectangle bounds, const char *label, int labelWidth, bool accent) {
    Color fill = accent ? Color {32, 101, 151, 235} : Color {19, 31, 48, 235};
    Color border = accent ? Color {88, 198, 255, 255} : Color {72, 104, 133, 255};
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
        fill = AddColor(fill, 22);
        border = WHITE;
    }
    DrawRectangleRounded(bounds, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.0f, border);
    DrawTextLine(font, label, static_cast<int>(bounds.x + (bounds.width - labelWidth) * 0.5f),
                 static_cast<int>(bounds.y + 9), 16, WHITE);
}

void DrawPanelBackground() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    DrawRectangle(width - static_cast<int>(kPanelWidth), 0, static_cast<int>(kPanelWidth), height,
                  Color {7, 15, 28, 238});
    DrawLine(width - static_cast<int>(kPanelWidth), 0, width - static_cast<int>(kPanelWidth), height,
             Color {47, 91, 124, 220});
}

void DrawViewerPanel(Font font, const PanelData &panel) {
    DrawPanelBackground();
    int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
    DrawTextLine(font, panel.title.c_str(), x, 25, 20, Color {137, 221, 255, 255});
    DrawTextLine(font, panel.name.c_str(), x, 58, 24, WHITE);
    DrawTextLine(font, panel.subtitle.c_str(), x, 89, 14, Color {161, 187, 208, 255});
    int y = 126;
    for (const auto &row: panel.rows) y = DrawKeyValue(font, row, y);
    if (!panel.detailRows.empty()) {
        DrawTextLine(font, "MINERAL VEINS", x, y + 8, 15, Color {123, 157, 183, 255});
        y += 34;
        for (const auto &row: panel.detailRows) {
            DrawTextLine(font, row.key, x, y, 14, RAYWHITE);
            int valueWidth = static_cast<int>(MeasureTextEx(font, row.value.c_str(), 14.0f, 1.0f).x);
            DrawTextLine(font, row.value.c_str(), GetScreenWidth() - 22 - valueWidth, y, 14,
                         Color {137, 221, 255, 255});
            y += 22;
        }
    }
    if (!panel.footerKey.empty())
        DrawTextLine(font, panel.footerKey.c_str(), x, y + 8, 15, Color {123, 157, 183, 255});
    if (!panel.footerValue.empty())
        DrawTextLine(font, panel.footerValue.c_str(), x, y + (panel.footerKey.empty() ? 16 : 31), 14,
                     panel.footerKey.empty() ? Color {98, 131, 159, 255} : RAYWHITE);
}

}
