#include "panel_scroll.hh"

#include <cassert>

int main() {
    using viewer::PanelScroll;

    assert(PanelScroll::MaxOffset(100.0f, 200.0f) == 0.0f);
    assert(PanelScroll::MaxOffset(300.0f, 200.0f) == 100.0f);
    assert(PanelScroll::ApplyWheel(0.0f, 3.0f, 300.0f, 200.0f) == 0.0f);
    assert(PanelScroll::ApplyWheel(0.0f, -1.0f, 300.0f, 200.0f) == 32.0f);
    assert(PanelScroll::ApplyWheel(0.0f, -4.0f, 300.0f, 200.0f) == 100.0f);
    assert(PanelScroll::ApplyWheel(100.0f, -3.0f, 300.0f, 200.0f) == 100.0f);
    assert(PanelScroll::ApplyWheel(0.0f, 3.0f, 100.0f, 200.0f) == 0.0f);

    const auto viewport = viewer::CalculatePanelViewport(682, 501, 112, 8);
    assert(viewport.footerTop == 570.0f);
    assert(viewport.mineralTop == 501.0f);
    assert(viewport.mineralHeight == 61.0f);
    assert(viewport.mineralTop + viewport.mineralHeight + 8.0f <= viewport.footerTop);

    const auto systemViewport = viewer::CalculatePanelViewport(682, 501, 125, 8);
    assert(systemViewport.footerTop == 557.0f);
    assert(systemViewport.mineralHeight == 48.0f);

    const float contentHeight = 363.0f;
    const float maxOffset = PanelScroll::MaxOffset(contentHeight, viewport.mineralHeight);
    const float lastRowTop = 34.0f + 21.0f + 13.0f * 22.0f;
    const float lastRowScreenTop = viewport.mineralTop + lastRowTop - maxOffset;
    assert(lastRowScreenTop >= viewport.mineralTop);
    assert(lastRowScreenTop < viewport.mineralTop + viewport.mineralHeight);

    assert(PanelScroll::LineFitsViewport(556.0f, 14.0f, 501.0f, 459.0f));
    assert(!PanelScroll::LineFitsViewport(556.0f, 14.0f, 501.0f, 40.0f));
    assert(PanelScroll::LineFitsViewport(lastRowScreenTop, 14.0f,
                                         viewport.mineralTop, viewport.mineralHeight));
}
