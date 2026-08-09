#include "panel_footer.hh"

#include <cassert>

int main() {
    assert(viewer::FormatPanelFooter("singularities", "none") == "singularities: none");

    assert(viewer::FormatPanelFooter("", "Original-style sprite lighting").empty());
}
