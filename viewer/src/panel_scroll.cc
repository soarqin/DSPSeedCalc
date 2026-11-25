#include "panel_scroll.hh"

#include <algorithm>

namespace viewer {

PanelViewport CalculatePanelViewport(int screenHeight, int mineralTop, int footerReserved, int gap) {
    const int footerTop = std::clamp(screenHeight - footerReserved, 0, screenHeight);
    const int mineralBottom = std::max(0, footerTop - gap);
    // When there isn't room for the table header and a row, scroll the metadata with the table.
    const bool scrollRows = mineralBottom - mineralTop < 72;
    const int scrollTop = scrollRows ? 0 : mineralTop;
    return PanelViewport {
        static_cast<float>(scrollTop),
        static_cast<float>(mineralBottom - scrollTop),
        static_cast<float>(footerTop),
        scrollRows,
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
