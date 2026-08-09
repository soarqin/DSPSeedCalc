#pragma once

namespace viewer {

struct PanelViewport {
    float mineralTop = 0.0f;
    float mineralHeight = 0.0f;
    float footerTop = 0.0f;
};

PanelViewport CalculatePanelViewport(int screenHeight, int mineralTop, int footerReserved, int gap);

struct PanelScroll {
    static float MaxOffset(float contentHeight, float viewportHeight);
    static float ApplyWheel(float offset, float wheel, float contentHeight, float viewportHeight);
    static bool LineFitsViewport(float lineTop, float lineHeight,
                                 float viewportTop, float viewportHeight);
};

}
