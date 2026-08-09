#include "viewer_ui.hh"
#include "panel_footer.hh"
#include "panel_scroll.hh"

#include <algorithm>

namespace viewer {
namespace {

constexpr float kPanelWidth = 326.0f;
constexpr int kPanelBottomReserved = 170;
constexpr int kPanelMineralGap = 8;

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

int MineralContentHeight(Font font, const PanelData &panel) {
    int height = 34 + 21;
    height += static_cast<int>(panel.mineralRows.size()) * 22;
    if (panel.mineralRows.empty()) height += 22;
    if (!panel.mineralStatus.empty()) height += 22;
    (void)font;
    return height;
}

void DrawMineralRows(Font font, const PanelData &panel, const Rectangle &bounds, float scrollOffset,
                     float contentHeight) {
    int x = static_cast<int>(bounds.x);
    int y = static_cast<int>(bounds.y - scrollOffset);
    int right = static_cast<int>(bounds.x + bounds.width);
    auto drawLine = [&](const char *text, int lineY, int size, Color color) {
        if (PanelScroll::LineFitsViewport(static_cast<float>(lineY), static_cast<float>(size),
                                          bounds.y, bounds.height))
            DrawTextLine(font, text, x, lineY, size, color);
    };
    auto drawAt = [&](const char *text, int lineX, int lineY, int size, Color color) {
        if (PanelScroll::LineFitsViewport(static_cast<float>(lineY), static_cast<float>(size),
                                          bounds.y, bounds.height))
            DrawTextLine(font, text, lineX, lineY, size, color);
    };
    drawLine(panel.mineralTitle.empty() ? "MINERALS" : panel.mineralTitle.c_str(),
             y + 8, 15, Color {123, 157, 183, 255});
    y += 34;
    drawLine("mineral", y, 12, Color {98, 131, 159, 255});
    drawAt("piles", x + 146, y, 12, Color {98, 131, 159, 255});
    drawAt("exact amount", right - 90, y, 12, Color {98, 131, 159, 255});
    y += 21;
    for (const auto &row: panel.mineralRows) {
        drawLine(row.key, y, 14, RAYWHITE);
        int pileWidth = static_cast<int>(MeasureTextEx(font, row.pileCount.c_str(), 14.0f, 1.0f).x);
        drawAt(row.pileCount.c_str(), x + 178 - pileWidth, y, 14, RAYWHITE);
        int amountWidth = static_cast<int>(MeasureTextEx(font, row.exactAmount.c_str(), 14.0f, 1.0f).x);
        drawAt(row.exactAmount.c_str(), right - amountWidth, y, 14,
               Color {137, 221, 255, 255});
        y += 22;
    }
    if (panel.mineralRows.empty()) {
        drawLine("None", y, 14, Color {178, 197, 213, 255});
        y += 22;
    }
    if (!panel.mineralStatus.empty()) {
        drawLine(panel.mineralStatus.c_str(), y + 2, 12, Color {255, 190, 116, 255});
    }

    if (contentHeight <= bounds.height) return;
    float thumbHeight = std::max(18.0f, bounds.height * bounds.height / contentHeight);
    float maxOffset = contentHeight - bounds.height;
    float thumbY = bounds.y + (bounds.height - thumbHeight) * scrollOffset / maxOffset;
    DrawRectangleRounded(Rectangle {bounds.x + bounds.width - 5.0f, bounds.y, 3.0f, bounds.height},
                         0.5f, 4, Color {25, 43, 61, 255});
    DrawRectangleRounded(Rectangle {bounds.x + bounds.width - 5.0f, thumbY, 3.0f, thumbHeight},
                         0.5f, 4, Color {88, 198, 255, 210});
}

}

ViewerPanelLayout GetViewerPanelLayout(Font font, const PanelData &panel, int footerReserved) {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    int x = width - static_cast<int>(kPanelWidth) + 22;
    int mineralTop = 126 + static_cast<int>(panel.rows.size()) * 25;
    PanelViewport viewport = CalculatePanelViewport(height, mineralTop, footerReserved, kPanelMineralGap);
    ViewerPanelLayout layout;
    layout.mineralBounds = Rectangle {static_cast<float>(x), viewport.mineralTop,
                                      kPanelWidth - 44.0f,
                                      viewport.mineralHeight};
    layout.footerBounds = Rectangle {static_cast<float>(x), viewport.footerTop,
                                     kPanelWidth - 44.0f, static_cast<float>(height - viewport.footerTop)};
    layout.mineralContentHeight = static_cast<float>(MineralContentHeight(font, panel));
    return layout;
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

void DrawViewerPanel(Font font, const PanelData &panel, float scrollOffset, int footerReserved) {
    DrawPanelBackground();
    int x = GetScreenWidth() - static_cast<int>(kPanelWidth) + 22;
    DrawTextLine(font, panel.title.c_str(), x, 25, 20, Color {137, 221, 255, 255});
    DrawTextLine(font, panel.name.c_str(), x, 58, 24, WHITE);
    DrawTextLine(font, panel.subtitle.c_str(), x, 89, 14, Color {161, 187, 208, 255});
    int y = 126;
    for (const auto &row: panel.rows) y = DrawKeyValue(font, row, y);
    ViewerPanelLayout layout = GetViewerPanelLayout(font, panel, footerReserved);
    float maxOffset = std::max(0.0f, layout.mineralContentHeight - layout.mineralBounds.height);
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxOffset);
    if (!panel.mineralTitle.empty() || !panel.mineralRows.empty() || !panel.mineralStatus.empty())
        DrawMineralRows(font, panel, layout.mineralBounds, scrollOffset, layout.mineralContentHeight);
    std::string footer = FormatPanelFooter(panel.footerKey, panel.footerValue);
    if (!footer.empty() && footerReserved >= kPanelBottomReserved)
        DrawTextLine(font, footer.c_str(), x, static_cast<int>(layout.footerBounds.y), 15,
                     Color {123, 157, 183, 255});
}

}
