#include "viewer_ui.hh"
#include "panel_footer.hh"
#include "panel_scroll.hh"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace viewer {
namespace {

constexpr int kPanelMineralGap = 8;
constexpr int kPanelTitleY = 22;
constexpr int kPanelNameY = 40;
constexpr int kPanelSubtitleY = 74;
constexpr int kPanelSeparatorY = 100;
constexpr int kPanelRowsTop = 110;
constexpr int kPanelRowHeight = 23;
constexpr int kPanelTextSize = 14;
constexpr int kMineralRowHeight = 21;
constexpr float kBodyListHeaderHeight = 36.0f;
constexpr float kBodyListRowHeight = 24.0f;

Color AddColor(Color color, int amount) {
    color.r = static_cast<unsigned char>(std::clamp(static_cast<int>(color.r) + amount, 0, 255));
    color.g = static_cast<unsigned char>(std::clamp(static_cast<int>(color.g) + amount, 0, 255));
    color.b = static_cast<unsigned char>(std::clamp(static_cast<int>(color.b) + amount, 0, 255));
    return color;
}

Color WithAlpha(Color color, float alpha) {
    color.a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * static_cast<float>(color.a));
    return color;
}

/* A row whose key and value cannot share one line puts the value on a second line. */
bool RowWraps(Font font, const PanelRow &row, float width) {
    float keyWidth = MeasureTextWidth(font, row.key, kPanelTextSize);
    float valueWidth = MeasureTextWidth(font, row.value.c_str(), kPanelTextSize);
    return keyWidth + 14.0f + valueWidth > width;
}

int RowHeight(Font font, const PanelRow &row, float width) {
    return RowWraps(font, row, width) ? kPanelRowHeight * 2 - 4 : kPanelRowHeight;
}

int RowsBottom(Font font, const PanelData &panel, float width) {
    int y = kPanelRowsTop;
    for (const auto &row: panel.rows) y += RowHeight(font, row, width);
    return y;
}

int DrawKeyValue(Font font, const PanelRow &row, int x, int right, int y, bool striped) {
    int height = RowHeight(font, row, static_cast<float>(right - x));
    if (striped)
        DrawRectangle(x - 8, y - 4, right - x + 16, height, Color {16, 30, 48, 120});
    DrawTextLine(font, row.key, x, y, kPanelTextSize, theme::kLabel);
    int valueY = height > kPanelRowHeight ? y + kPanelRowHeight - 4 : y;
    DrawTextRight(font, row.value.c_str(), right, valueY, kPanelTextSize, theme::kText);
    return y + height;
}

int MineralContentHeight(const PanelData &panel) {
    int height = 30 + 20;
    height += static_cast<int>(panel.mineralRows.size()) * kMineralRowHeight;
    if (panel.mineralRows.empty()) height += kMineralRowHeight;
    if (!panel.mineralStatus.empty()) height += 22;
    return height;
}

void DrawMineralRows(Font font, const PanelData &panel, int x, int right, int y, Rectangle viewport) {
    int pileRight = x + (right - x) * 55 / 100;
    auto visible = [&](int top, int height) {
        return PanelScroll::LineFitsViewport(static_cast<float>(top), static_cast<float>(height),
                                             viewport.y, viewport.height);
    };
    if (visible(y + 6, 13)) {
        DrawTextLine(font, panel.mineralTitle.empty() ? "MINERALS" : panel.mineralTitle.c_str(),
                     x, y + 6, 13, theme::kAccentSoft);
        DrawLine(x, y + 25, right, y + 25, theme::kSeparator);
    }
    y += 30;
    if (visible(y, 12)) {
        DrawTextLine(font, "vein", x, y, 12, theme::kLabel);
        DrawTextRight(font, "piles", pileRight, y, 12, theme::kLabel);
        DrawTextRight(font, "exact amount", right, y, 12, theme::kLabel);
    }
    y += 20;
    for (const auto &row: panel.mineralRows) {
        if (visible(y, kPanelTextSize)) {
            DrawTextLine(font, row.key, x, y, kPanelTextSize, theme::kText);
            DrawTextRight(font, row.pileCount.c_str(), pileRight, y, kPanelTextSize, theme::kTextDim);
            DrawTextRight(font, row.exactAmount.c_str(), right, y, kPanelTextSize, theme::kAccentSoft);
        }
        y += kMineralRowHeight;
    }
    if (panel.mineralRows.empty()) {
        if (visible(y, kPanelTextSize)) DrawTextLine(font, "None", x, y, kPanelTextSize, theme::kTextDim);
        y += kMineralRowHeight;
    }
    if (!panel.mineralStatus.empty() && visible(y + 2, 12))
        DrawTextLine(font, panel.mineralStatus.c_str(), x, y + 2, 12, theme::kWarn);
}

void DrawScrollBar(Rectangle bounds, float contentHeight, float scrollOffset) {
    if (contentHeight <= bounds.height || bounds.height <= 0) return;
    float thumbHeight = std::clamp(bounds.height * bounds.height / contentHeight,
                                   std::min(18.0f, bounds.height), bounds.height);
    float maxOffset = contentHeight - bounds.height;
    float thumbY = bounds.y + (bounds.height - thumbHeight) * scrollOffset / maxOffset;
    DrawRectangleRounded(Rectangle {bounds.x + bounds.width - 4.0f, bounds.y, 3.0f, bounds.height},
                         1.0f, 4, Color {25, 43, 61, 255});
    DrawRectangleRounded(Rectangle {bounds.x + bounds.width - 4.0f, thumbY, 3.0f, thumbHeight},
                         1.0f, 4, WithAlpha(theme::kAccent, 0.82f));
}

/* Greedy word wrap; returns the y below the last line. */
int DrawWrappedText(Font font, const std::string &text, int x, int y, int width, int size, Color color) {
    std::string line;
    std::size_t position = 0;
    while (position < text.size()) {
        std::size_t next = text.find(' ', position);
        if (next == std::string::npos) next = text.size();
        std::string word = text.substr(position, next - position);
        std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureTextWidth(font, candidate.c_str(), size) > static_cast<float>(width)) {
            DrawTextLine(font, line.c_str(), x, y, size, color);
            y += size + 6;
            line = word;
        } else {
            line = candidate;
        }
        position = next + 1;
    }
    if (!line.empty()) {
        DrawTextLine(font, line.c_str(), x, y, size, color);
        y += size + 6;
    }
    return y;
}

/* raylib's BeginScissorMode() reads its cached framebuffer size, which the post-process pass leaves
   stale because it binds framebuffers through rlgl directly; derive the rectangle from the render size. */
void BeginClip(Rectangle bounds) {
    rlDrawRenderBatchActive();
    float scaleX = static_cast<float>(GetRenderWidth()) / static_cast<float>(std::max(1, GetScreenWidth()));
    float scaleY = static_cast<float>(GetRenderHeight()) / static_cast<float>(std::max(1, GetScreenHeight()));
    rlEnableScissorTest();
    rlScissor(static_cast<int>(bounds.x * scaleX),
              static_cast<int>(static_cast<float>(GetRenderHeight()) - (bounds.y + bounds.height) * scaleY),
              static_cast<int>(bounds.width * scaleX), static_cast<int>(bounds.height * scaleY));
}

void EndClip() {
    rlDrawRenderBatchActive();
    rlDisableScissorTest();
}

void DrawCard(Rectangle bounds, float alpha = 1.0f) {
    DrawRectangleRounded(bounds, 0.06f, 8, WithAlpha(theme::kPanelFill, alpha));
    DrawRectangleRoundedLinesEx(bounds, 0.06f, 8, 1.0f, WithAlpha(theme::kPanelBorder, alpha));
}

}

ViewerPanelLayout GetViewerPanelLayout(Font font, const PanelData &panel, Rectangle bounds, int footerReserved) {
    float width = bounds.width - 2 * kPanelPadding;
    int mineralTop = RowsBottom(font, panel, width) + 10;
    PanelViewport viewport = CalculatePanelViewport(static_cast<int>(bounds.height), mineralTop, footerReserved,
                                                   kPanelMineralGap);
    ViewerPanelLayout layout;
    layout.scrollBounds = {bounds.x, bounds.y + viewport.scrollTop, bounds.width, viewport.scrollHeight};
    layout.footerBounds = {bounds.x + kPanelPadding, bounds.y + viewport.footerTop,
                           width, bounds.height - viewport.footerTop};
    layout.scrollRows = viewport.scrollRows;
    layout.contentHeight = static_cast<float>(MineralContentHeight(panel) + (layout.scrollRows ? mineralTop : 0));
    return layout;
}

float MeasureTextWidth(Font font, const char *text, int size) {
    if (text == nullptr || text[0] == '\0') return 0.0f;
    return MeasureTextEx(font, text, static_cast<float>(size), 1.0f).x;
}

void DrawTextLine(Font font, const char *text, int x, int y, int size, Color color) {
    DrawTextEx(font, text, Vector2 {static_cast<float>(x), static_cast<float>(y)},
               static_cast<float>(size), 1.0f, color);
}

void DrawTextRight(Font font, const char *text, int right, int y, int size, Color color) {
    int width = static_cast<int>(std::ceil(MeasureTextWidth(font, text, size)));
    DrawTextLine(font, text, right - width, y, size, color);
}

void DrawHeading(Font font, const std::string &text, int x, int y, int size, float width, Color color) {
    while (size > 15 && MeasureTextWidth(font, text.c_str(), size) > width) --size;
    if (MeasureTextWidth(font, text.c_str(), size) <= width) {
        DrawTextLine(font, text.c_str(), x, y, size, color);
        return;
    }
    std::string shortened = text;
    float available = width - MeasureTextWidth(font, "...", size);
    while (!shortened.empty() && MeasureTextWidth(font, shortened.c_str(), size) > available) {
        std::size_t last = shortened.size() - 1;
        while (last > 0 && (static_cast<unsigned char>(shortened[last]) & 0xc0) == 0x80) --last;
        shortened.resize(last);
    }
    DrawTextLine(font, (shortened + "...").c_str(), x, y, size, color);
}

void DrawViewerButton(Font font, Rectangle bounds, const char *label, bool accent, bool enabled) {
    Color fill = accent ? Color {32, 101, 151, 235} : theme::kFieldFill;
    Color border = accent ? theme::kAccent : theme::kFieldBorder;
    Color text = theme::kText;
    if (!enabled) {
        fill = theme::kFieldFill;
        border = Color {44, 64, 84, 255};
        text = theme::kHint;
    } else if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
        fill = AddColor(fill, IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 8 : 22);
        border = WHITE;
    }
    DrawRectangleRounded(bounds, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.0f, border);
    int size = bounds.height >= 36.0f ? 15 : 14;
    float labelWidth = MeasureTextWidth(font, label, size);
    DrawTextLine(font, label, static_cast<int>(bounds.x + (bounds.width - labelWidth) * 0.5f),
                 static_cast<int>(bounds.y + (bounds.height - static_cast<float>(size)) * 0.5f), size, text);
}

void DrawPanelBackground(Rectangle bounds) {
    int left = static_cast<int>(bounds.x);
    int top = static_cast<int>(bounds.y);
    int height = static_cast<int>(bounds.height);
    DrawRectangleRec(bounds, theme::kPanelFill);
    DrawRectangleGradientH(left - 18, top, 18, height, Color {7, 15, 28, 0}, Color {7, 15, 28, 110});
    DrawLine(left, top, left, top + height, theme::kPanelBorder);
    DrawRectangle(left, top, static_cast<int>(bounds.width), 2, WithAlpha(theme::kAccent, 0.75f));
}

void DrawViewerPanel(Font font, const PanelData &panel, Rectangle bounds, float scrollOffset, int footerReserved) {
    DrawPanelBackground(bounds);
    auto layout = GetViewerPanelLayout(font, panel, bounds, footerReserved);
    scrollOffset = std::clamp(scrollOffset, 0.0f, PanelScroll::MaxOffset(layout.contentHeight, layout.scrollBounds.height));
    int x = static_cast<int>(layout.footerBounds.x);
    int right = static_cast<int>(layout.footerBounds.x + layout.footerBounds.width);
    int top = static_cast<int>(bounds.y - (layout.scrollRows ? scrollOffset : 0));
    if (layout.scrollRows) BeginClip(layout.scrollBounds);
    DrawTextLine(font, panel.title.c_str(), x, top + kPanelTitleY, 13, theme::kAccentSoft);
    DrawHeading(font, panel.name, x, top + kPanelNameY, 26, layout.footerBounds.width, theme::kText);
    DrawTextLine(font, panel.subtitle.c_str(), x, top + kPanelSubtitleY, 15, theme::kTextDim);
    DrawLine(x, top + kPanelSeparatorY, right, top + kPanelSeparatorY, theme::kSeparator);
    int y = top + kPanelRowsTop;
    bool striped = false;
    for (const auto &row: panel.rows) {
        y = DrawKeyValue(font, row, x, right, y, striped);
        striped = !striped;
    }
    if (!layout.scrollRows) {
        BeginClip(layout.scrollBounds);
        y -= static_cast<int>(scrollOffset);
    }
    if (!panel.mineralTitle.empty() || !panel.mineralRows.empty() || !panel.mineralStatus.empty())
        DrawMineralRows(font, panel, x, right, y + 10, layout.scrollBounds);
    EndClip();
    auto bar = layout.scrollBounds;
    bar.width -= 6;
    DrawScrollBar(bar, layout.contentHeight, scrollOffset);
    std::string footer = FormatPanelFooter(panel.footerKey, panel.footerValue);
    if (!footer.empty())
        DrawWrappedText(font, footer, x, static_cast<int>(layout.footerBounds.y),
                        static_cast<int>(layout.footerBounds.width), kPanelTextSize, theme::kLabel);
}

BodyListLayout GetBodyListLayout(const ViewerLayout &screen, std::size_t entryCount) {
    BodyListLayout layout;
    layout.rowHeight = screen.compact ? 44.0f : kBodyListRowHeight;
    layout.contentHeight = static_cast<float>(entryCount) * layout.rowHeight;
    layout.bounds = screen.map;
    if (!screen.compact) {
        float maxHeight = screen.map.height - kHintBarHeight - 3 * kBodyListMargin;
        layout.bounds = {kBodyListMargin, screen.map.y + kBodyListMargin, kBodyListWidth,
                         std::min(kBodyListHeaderHeight + layout.contentHeight + 8.0f, std::max(0.0f, maxHeight))};
    }
    layout.rows = {layout.bounds.x + 6, layout.bounds.y + kBodyListHeaderHeight, layout.bounds.width - 12,
                   std::max(0.0f, layout.bounds.height - kBodyListHeaderHeight - 6)};
    return layout;
}

std::size_t BodyListHitTest(const BodyListLayout &layout, std::size_t entryCount, float scrollOffset,
                            Vector2 mouse) {
    if (!CheckCollisionPointRec(mouse, layout.rows)) return entryCount;
    float local = mouse.y - layout.rows.y + scrollOffset;
    if (local < 0.0f) return entryCount;
    auto index = static_cast<std::size_t>(local / layout.rowHeight);
    return index < entryCount ? index : entryCount;
}

void DrawBodyList(Font font, const BodyListLayout &layout, const char *title, const std::vector<BodyListEntry> &entries,
                  float scrollOffset, std::size_t selected, std::size_t hovered) {
    DrawCard(layout.bounds, 0.94f);
    int x = static_cast<int>(layout.bounds.x + 14.0f);
    int right = static_cast<int>(layout.bounds.x + layout.bounds.width - 14.0f);
    DrawTextLine(font, title, x, static_cast<int>(layout.bounds.y + 12.0f), 13, theme::kAccentSoft);
    std::string count = std::to_string(entries.size());
    DrawTextRight(font, count.c_str(), right, static_cast<int>(layout.bounds.y + 12.0f), 13, theme::kHint);
    DrawLine(x, static_cast<int>(layout.rows.y - 4.0f), right, static_cast<int>(layout.rows.y - 4.0f),
             theme::kSeparator);

    bool scrollable = layout.contentHeight > layout.rows.height;
    BeginClip(layout.rows);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        float rowY = layout.rows.y + static_cast<float>(i) * layout.rowHeight - scrollOffset;
        if (rowY + layout.rowHeight < layout.rows.y || rowY > layout.rows.y + layout.rows.height) continue;
        const auto &entry = entries[i];
        Rectangle row {layout.rows.x, rowY + 1.0f, layout.rows.width - (scrollable ? 8.0f : 0.0f),
                       layout.rowHeight - 2.0f};
        if (i == selected) DrawRectangleRounded(row, 0.3f, 4, theme::kRowSelected);
        else if (i == hovered) DrawRectangleRounded(row, 0.3f, 4, theme::kRowHover);
        float indent = static_cast<float>(entry.indent) * 14.0f;
        float centerY = rowY + layout.rowHeight * 0.5f;
        if (entry.indent > 0)
            DrawLine(static_cast<int>(row.x + 12.0f + indent - 9.0f), static_cast<int>(centerY),
                     static_cast<int>(row.x + 12.0f + indent - 5.0f), static_cast<int>(centerY), theme::kHint);
        DrawCircleV(Vector2 {row.x + 12.0f + indent, centerY}, 4.0f, entry.swatch);
        int textY = static_cast<int>(centerY - 7.0f);
        int detailRight = static_cast<int>(row.x + row.width - 8.0f);
        float detailWidth = MeasureTextWidth(font, entry.detail.c_str(), 12);
        DrawTextLine(font, entry.label.c_str(), static_cast<int>(row.x + 24.0f + indent), textY, 14,
                     i == selected ? WHITE : theme::kText);
        if (row.x + 24.0f + indent + MeasureTextWidth(font, entry.label.c_str(), 14) + 8.0f <
            static_cast<float>(detailRight) - detailWidth)
            DrawTextRight(font, entry.detail.c_str(), detailRight, textY + 1, 12,
                          i == selected ? theme::kAccentSoft : theme::kHint);
    }
    EndClip();

    DrawScrollBar(layout.rows, layout.contentHeight, scrollOffset);
}

void DrawHintBar(Font font, const char *text) {
    float width = MeasureTextWidth(font, text, 13) + 28.0f;
    Rectangle bounds {kBodyListMargin, static_cast<float>(GetScreenHeight()) - kBodyListMargin - kHintBarHeight,
                      width, kHintBarHeight};
    DrawRectangleRounded(bounds, 0.3f, 6, Color {5, 12, 23, 200});
    DrawTextLine(font, text, static_cast<int>(bounds.x + 14.0f), static_cast<int>(bounds.y + 10.0f), 13,
                 theme::kTextDim);
}

void DrawToast(Font font, Rectangle map, const char *text, Color color, float alpha) {
    if (alpha <= 0.0f) return;
    float ideal = MeasureTextWidth(font, text, 15) + 36.0f;
    float width = std::min(ideal, map.width - 16.0f);
    Rectangle bounds {map.x + (map.width - width) * 0.5f, map.y + 12, width, width < ideal ? 58.0f : 36.0f};
    DrawRectangleRounded(bounds, 0.3f, 6, WithAlpha(Color {9, 19, 34, 235}, alpha));
    DrawRectangleRoundedLinesEx(bounds, 0.3f, 6, 1.0f, WithAlpha(color, alpha));
    DrawWrappedText(font, text, static_cast<int>(bounds.x + 18), static_cast<int>(bounds.y + 10),
                    static_cast<int>(width - 36), 15, WithAlpha(color, alpha));
}

void DrawHelpOverlay(Font font, ViewMode mode) {
    struct HelpLine {
        const char *keys;
        const char *action;
    };
    static const HelpLine galaxyLines[] = {
        {"Left click", "Select a star"},
        {"Enter / 2 clicks", "Open system"},
        {"Left / middle drag", "Orbit camera"},
        {"Mouse wheel", "Zoom"},
        {"Arrow keys", "Orbit the camera"},
        {"Tab / Shift+Tab", "Next / previous star"},
        {"B", "Select birth star"},
        {"R", "Random seed"},
        {"Z", "Reset the camera"},
        {"L", "Toggle star list"},
        {"Esc", "Clear the selection"},
    };
    static const HelpLine systemLines[] = {
        {"Left click", "Select a planet"},
        {"F / 2 clicks", "Follow planet"},
        {"Left / middle drag", "Orbit camera"},
        {"Right drag", "Roll the camera"},
        {"Mouse wheel", "Zoom"},
        {"Tab / Shift+Tab", "Next / previous planet"},
        {"Space", "Pause / resume"},
        {"- / =", "Orbit speed"},
        {"Z", "Center on star"},
        {"L", "Toggle planet list"},
        {"Esc / G / Backspace", "Galaxy map"},
    };
    const HelpLine *lines = mode == ViewMode::Galaxy ? galaxyLines : systemLines;
    std::size_t count = mode == ViewMode::Galaxy ? std::size(galaxyLines) : std::size(systemLines);

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color {0, 0, 0, 140});
    float width = std::min(520.0f, static_cast<float>(GetScreenWidth()) - 16);
    int step = std::clamp((GetScreenHeight() - 108) / static_cast<int>(count), 16, 28);
    float height = 92.0f + static_cast<float>(count * step);
    Rectangle card {(static_cast<float>(GetScreenWidth()) - width) * 0.5f,
                    (static_cast<float>(GetScreenHeight()) - height) * 0.5f, width, height};
    DrawCard(card);
    DrawRectangle(static_cast<int>(card.x + 1.0f), static_cast<int>(card.y + 1.0f),
                  static_cast<int>(card.width - 2.0f), 2, WithAlpha(theme::kAccent, 0.75f));
    int padding = width < 440 ? 12 : 24;
    int x = static_cast<int>(card.x) + padding;
    int y = static_cast<int>(card.y + 16);
    DrawTextLine(font, mode == ViewMode::Galaxy ? "Galaxy controls" : "System controls", x, y, 22, theme::kText);
    y += 36;
    int size = width < 440 ? 13 : 15;
    int actionX = static_cast<int>(card.x + width * 0.51f);
    for (std::size_t i = 0; i < count; ++i) {
        DrawTextLine(font, lines[i].keys, x, y, size, theme::kAccentSoft);
        DrawTextLine(font, lines[i].action, actionX, y, size, theme::kText);
        y += step;
    }
    DrawTextLine(font, "Click or press Esc to close", x, static_cast<int>(card.y + card.height - 24.0f), 13,
                 theme::kHint);
}

void DrawBusyOverlay(Font font, const char *text) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color {0, 0, 0, 120});
    float width = MeasureTextWidth(font, text, 18) + 64.0f;
    Rectangle card {(static_cast<float>(GetScreenWidth()) - width) * 0.5f,
                    static_cast<float>(GetScreenHeight()) * 0.5f - 30.0f, width, 60.0f};
    DrawCard(card);
    DrawTextLine(font, text, static_cast<int>(card.x + 32.0f), static_cast<int>(card.y + 21.0f), 18,
                 theme::kAccentSoft);
}

}
