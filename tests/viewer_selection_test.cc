#include "viewer_selection.hh"

#include <cassert>
#include <limits>

int main() {
    assert(viewer::DefaultSelectedStarIndex(0) == std::numeric_limits<std::size_t>::max());
    assert(viewer::DefaultSelectedStarIndex(32) == 0);
    assert(viewer::DefaultSelectedStarIndex(64) == 0);
}
