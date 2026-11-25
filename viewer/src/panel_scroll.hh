#pragma once

namespace viewer {

struct PanelViewport {
    float scrollTop = 0.0f;
    float scrollHeight = 0.0f;
    float footerTop = 0.0f;
    bool scrollRows = false;
};

PanelViewport CalculatePanelViewport(int screenHeight, int mineralTop, int footerReserved, int gap);

struct PanelScroll {
    static float MaxOffset(float contentHeight, float viewportHeight);
    static float ApplyWheel(float offset, float wheel, float contentHeight, float viewportHeight);
    static bool LineFitsViewport(float lineTop, float lineHeight,
                                 float viewportTop, float viewportHeight);
};

}
