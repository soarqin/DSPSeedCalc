#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dspugen {

inline constexpr std::size_t VeinTypeCount = 15;

struct VeinAmountSummary {
    std::array<std::uint32_t, VeinTypeCount> nodeCount{};
    std::array<std::int64_t, VeinTypeCount> baseAmount{};
};

}
