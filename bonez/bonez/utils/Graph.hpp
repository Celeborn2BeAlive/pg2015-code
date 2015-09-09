#pragma once

#include <vector>
#include <cstdint>
#include <limits>

namespace BnZ {

static const uint32_t UNDEFINED_NODE = std::numeric_limits<uint32_t>::max();

typedef uint32_t GraphNodeIndex;

typedef std::vector<GraphNodeIndex> GraphAdjacencyList;
typedef std::vector<GraphAdjacencyList> Graph;

}
