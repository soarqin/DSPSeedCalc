#include "panel_scroll.hh"

#include <algorithm>

namespace viewer {

PanelViewport CalculatePanelViewport(int screenHeight, int mineralTop, int footerReserved, int gap) {
    const int footerTop = std::clamp(screenHeight - footerReserved, 0, screenHeight);
    const int mineralBottom = std::max(0, footerTop - gap);
    const int visibleMineralTop = std::min(mineralTop, std::max(0, mineralBottom - 1));
    return PanelViewport {
        static_cast<float>(visibleMineralTop),
        static_cast<float>(std::max(1, mineralBottom - visibleMineralTop)),
        static_cast<float>(footerTop),
    };
}

float PanelScroll::MaxOffset(float contentHeight, float viewportHeight) {
    return std::max(0.0f, contentHeight - viewportHeight);
}

float PanelScroll::ApplyWheel(float offset, float wheel, float contentHeight, float viewportHeight) {
    float maxOffset = MaxOffset(contentHeight, viewportHeight);
    return std::clamp(offset - wheel * 32.0f, 0.0f, maxOffset);
}

bool PanelScroll::LineFitsViewport(float lineTop, float lineHeight,
                                   float viewportTop, float viewportHeight) {
    return lineTop >= viewportTop && lineTop + lineHeight <= viewportTop + viewportHeight;
}

}
