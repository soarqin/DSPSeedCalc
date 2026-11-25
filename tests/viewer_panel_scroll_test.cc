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

    const auto viewport = viewer::CalculatePanelViewport(782, 501, 112, 8);
    assert(!viewport.scrollRows);
    assert(viewport.footerTop == 670.0f);
    assert(viewport.scrollTop == 501.0f);
    assert(viewport.scrollHeight == 161.0f);
    assert(viewport.scrollTop + viewport.scrollHeight + 8.0f <= viewport.footerTop);

    const auto systemViewport = viewer::CalculatePanelViewport(682, 501, 125, 8);
    assert(systemViewport.scrollRows);
    assert(systemViewport.footerTop == 557.0f);
    assert(systemViewport.scrollTop == 0.0f);
    assert(systemViewport.scrollHeight == 549.0f);

    const auto shortViewport = viewer::CalculatePanelViewport(240, 501, 112, 8);
    assert(shortViewport.scrollRows && shortViewport.scrollHeight == 120.0f);
    assert(PanelScroll::MaxOffset(501 + 363, shortViewport.scrollHeight) == 744.0f);

    const float contentHeight = 363.0f;
    const float maxOffset = PanelScroll::MaxOffset(contentHeight, viewport.scrollHeight);
    const float lastRowTop = 34.0f + 21.0f + 13.0f * 22.0f;
    const float lastRowScreenTop = viewport.scrollTop + lastRowTop - maxOffset;
    assert(lastRowScreenTop >= viewport.scrollTop);
    assert(lastRowScreenTop < viewport.scrollTop + viewport.scrollHeight);

    assert(PanelScroll::LineFitsViewport(556.0f, 14.0f, 501.0f, 459.0f));
    assert(!PanelScroll::LineFitsViewport(556.0f, 14.0f, 501.0f, 40.0f));
    assert(PanelScroll::LineFitsViewport(lastRowScreenTop, 14.0f,
                                         viewport.scrollTop, viewport.scrollHeight));
}
