#include "CurvilinearSkeleton.hpp"
#include "SkeletonLineSet.hpp"

#include <bonez/utils/itertools/itertools.hpp>
#include <queue>

#include <bonez/utils/MultiDimensionalArray.hpp>

namespace BnZ {

/**
 * Compute a list of node which are local minimas in their neighbour for the maxball radius.
 */
NodeVector computeLocalMinimas(const CurvilinearSkeleton& skeleton) {
    Graph graph = computeMaxBallGraph(skeleton);
    NodeVector nodes;
    
    for(GraphNodeIndex idx = 0, end = skeleton.size(); idx != end; ++idx) {
        bool isMinima = true;
        for(GraphNodeIndex n: graph[idx]) {
            if(skeleton[n].maxball < skeleton[idx].maxball) {
                isMinima = false;
                break;
            }
        }
        if(isMinima) {
            nodes.push_back(idx);
        }
    }
    
    return nodes;
}

void CurvilinearSkeleton::blur() {
    NodeArray nodes = m_Nodes;
    for(auto& node: range(size())) {
        Vec3f sumPositions(0.f);
        float sumMaxballs(0.f);
        for(const auto& neighbour: m_Graph[node]) {
            sumPositions += nodes[neighbour].P;
            sumMaxballs += nodes[neighbour].maxball;
        }
        sumPositions /= (float) m_Graph[node].size();
        sumMaxballs /= (float) m_Graph[node].size();
        m_Nodes[node] = Node(sumPositions, sumMaxballs);
    }
}

void CurvilinearSkeleton::blurMaxballs() {
    NodeArray nodes = m_Nodes;
    for(auto& node: range(size())) {
        float sumMaxballs(0.f);
        for(const auto& neighbour: m_Graph[node]) {
            sumMaxballs += nodes[neighbour].maxball;
        }
        sumMaxballs /= (float) m_Graph[node].size();
        m_Nodes[node] = Node(nodes[node].P, sumMaxballs);
    }
}

CurvilinearSkeleton computeMaxballBasedSegmentedSkeleton(const CurvilinearSkeleton& skeleton) {
    std::vector<GraphNodeIndex> sortedNodes;
    sortedNodes.reserve(skeleton.size());
    for(auto i = 0u; i < skeleton.size(); ++i) {
        sortedNodes.emplace_back(i);
    }
    std::sort(begin(sortedNodes), end(sortedNodes), [&](GraphNodeIndex n1, GraphNodeIndex n2) {
        if(skeleton.getNode(n1).maxball > skeleton.getNode(n2).maxball) {
            return true;
        } else if(skeleton.getNode(n1).maxball == skeleton.getNode(n2).maxball) {
            return skeleton.neighbours(n1).size() > skeleton.neighbours(n2).size();
        }
        return false;
    });

    std::vector<GraphNodeIndex> representatives;
    std::vector<GraphNodeIndex> representativeOf(skeleton.size(), UNDEFINED_NODE);

    for(auto nodeIndex: sortedNodes) {
        if(representativeOf[nodeIndex] == UNDEFINED_NODE) {
            auto newIndex = representatives.size();
            representativeOf[nodeIndex] = newIndex;
            representatives.emplace_back(nodeIndex);
            auto node = skeleton.getNode(nodeIndex);

            for(auto otherNodeIndex: range(skeleton.size())) {
                if(representativeOf[otherNodeIndex] == UNDEFINED_NODE) {
                    auto d = distanceSquared(skeleton.getNode(otherNodeIndex).P, node.P);
                    if(d < sqr(node.maxball)) {
                        representativeOf[otherNodeIndex] = newIndex;
                    }
                }
            }
        }
    }

    MultiDimensionalArray<uint8_t, 2> adjacencyMatrix(representatives.size(), representatives.size());
    std::fill(std::begin(adjacencyMatrix), std::end(adjacencyMatrix), 0u);

    for(auto nodeIndex: range(skeleton.size())) {
        auto representative = representativeOf[nodeIndex];
        for(auto neighbour: skeleton.neighbours(nodeIndex)) {
            auto neighbourRepresentative = representativeOf[neighbour];
            if(representative != neighbourRepresentative) {
                adjacencyMatrix(representative, neighbourRepresentative) =
                        adjacencyMatrix(neighbourRepresentative, representative) = 1u;
            }
        }
    }

    Graph segmentedGraph(representatives.size());
    for(auto newNodeIndex: range(representatives.size())) {
        for(auto potentialNeighbour: range(representatives.size())) {
            if(adjacencyMatrix(newNodeIndex, potentialNeighbour)) {
                segmentedGraph[newNodeIndex].emplace_back(potentialNeighbour);
            }
        }
    }

    CurvilinearSkeleton segmentedSkeleton;

    for(auto newNodeIndex: range(representatives.size())) {
        auto node = skeleton.getNode(representatives[newNodeIndex]);
        segmentedSkeleton.addNode(node.P, node.maxball);
    }
    segmentedSkeleton.setGraph(segmentedGraph);
    segmentedSkeleton.setWorldToGrid(skeleton.getWorldToGridMatrix(),
                                     skeleton.getWorldToGridScale());

    Grid3D<GraphNodeIndex> grid = skeleton.getGrid();
    for(GraphNodeIndex& nodeIndex: grid) {
        if(nodeIndex != UNDEFINED_NODE) {
            nodeIndex = representativeOf[nodeIndex];
        }
    }

    segmentedSkeleton.setGrid(grid);

    return segmentedSkeleton;
}


}
