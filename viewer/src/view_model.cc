#include "view_model.hh"

#include "galaxy.hh"

#include <algorithm>

namespace viewer {

void SortedAdd(std::vector<std::size_t> &nodes, std::size_t node) {
    auto position = std::lower_bound(nodes.begin(), nodes.end(), node);
    if (position == nodes.end() || *position != node) nodes.insert(position, node);
}

void RemoveNode(std::vector<std::size_t> &nodes, std::size_t node) {
    auto position = std::lower_bound(nodes.begin(), nodes.end(), node);
    if (position != nodes.end() && *position == node) nodes.erase(position);
}

std::vector<GalaxyLineData> CreateGalaxyLines(const dspugen::Galaxy *galaxy,
                                              const std::vector<StarViewData> &stars) {
    struct GraphNode {
        std::vector<std::size_t> connections;
        std::vector<std::size_t> lines;
    };
    std::vector<GraphNode> nodes(stars.size());
    std::vector<int> state(stars.size());
    for (std::size_t nodeIndex = 0; nodeIndex < stars.size(); ++nodeIndex) {
        const auto &position = galaxy->stars[nodeIndex]->position;
        for (std::size_t otherIndex = 0; otherIndex < nodeIndex; ++otherIndex) {
            const auto &otherPosition = galaxy->stars[otherIndex]->position;
            double dx = position.x - otherPosition.x;
            double dy = position.y - otherPosition.y;
            double dz = position.z - otherPosition.z;
            if (dx * dx + dy * dy + dz * dz >= 64.0) continue;
            SortedAdd(nodes[nodeIndex].connections, otherIndex);
            SortedAdd(nodes[otherIndex].connections, nodeIndex);
        }

        std::fill(state.begin(), state.end(), 0);
        Vector3 nodePosition = stars[nodeIndex].position;
        const auto &connections = nodes[nodeIndex].connections;
        for (std::size_t i = 0; i < connections.size(); ++i) {
            std::size_t firstIndex = connections[i];
            Vector3 firstPosition = stars[firstIndex].position;
            for (std::size_t j = i + 1; j < connections.size(); ++j) {
                std::size_t secondIndex = connections[j];
                if (!std::binary_search(nodes[firstIndex].connections.begin(),
                                        nodes[firstIndex].connections.end(), secondIndex)) continue;
                Vector3 secondPosition = stars[secondIndex].position;
                float firstDistance = Vector3DistanceSqr(firstPosition, nodePosition);
                float secondDistance = Vector3DistanceSqr(secondPosition, nodePosition);
                float otherDistance = Vector3DistanceSqr(firstPosition, secondPosition);
                float largest = std::max(firstDistance, std::max(secondDistance, otherDistance));
                float smallest = std::min(firstDistance, std::min(secondDistance, otherDistance));
                float middle = (firstDistance + secondDistance + otherDistance - largest - smallest) * 1.001f;
                smallest *= 1.01f;
                if (firstDistance <= middle || firstDistance <= smallest) {
                    if (state[i] == 0) {
                        SortedAdd(nodes[nodeIndex].lines, firstIndex);
                        SortedAdd(nodes[firstIndex].lines, nodeIndex);
                        state[i] = 1;
                    }
                } else {
                    state[i] = -1;
                    RemoveNode(nodes[nodeIndex].lines, firstIndex);
                    RemoveNode(nodes[firstIndex].lines, nodeIndex);
                }
                if (secondDistance <= middle || secondDistance <= smallest) {
                    if (state[j] == 0) {
                        SortedAdd(nodes[nodeIndex].lines, secondIndex);
                        SortedAdd(nodes[secondIndex].lines, nodeIndex);
                        state[j] = 1;
                    }
                } else {
                    state[j] = -1;
                    RemoveNode(nodes[nodeIndex].lines, secondIndex);
                    RemoveNode(nodes[secondIndex].lines, nodeIndex);
                }
                if (otherDistance > middle && otherDistance > smallest) {
                    RemoveNode(nodes[firstIndex].lines, secondIndex);
                    RemoveNode(nodes[secondIndex].lines, firstIndex);
                }
            }
            if (state[i] == 0) {
                SortedAdd(nodes[nodeIndex].lines, firstIndex);
                SortedAdd(nodes[firstIndex].lines, nodeIndex);
                state[i] = 1;
            }
        }
    }

    std::vector<GalaxyLineData> result;
    for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
        for (std::size_t otherIndex: nodes[nodeIndex].lines) {
            if (otherIndex > nodeIndex) result.push_back(GalaxyLineData {nodeIndex, otherIndex});
        }
    }
    return result;
}

}
