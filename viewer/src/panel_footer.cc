#include "panel_footer.hh"

namespace viewer {

std::string FormatPanelFooter(const std::string &key, const std::string &value) {
    if (key.empty()) return {};
    if (value.empty()) return key;
    return key + ": " + value;
}

}
