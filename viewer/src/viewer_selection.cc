#include "viewer_selection.hh"

#include <limits>

namespace viewer {

std::size_t DefaultSelectedStarIndex(std::size_t starCount) {
    return starCount == 0 ? std::numeric_limits<std::size_t>::max() : 0;
}

}
