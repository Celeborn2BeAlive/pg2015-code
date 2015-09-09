#pragma once

#include <bonez/types.hpp>
#include <bonez/maths/maths.hpp>
#include <bonez/utils/Graph.hpp>

#include <vector>

namespace BnZ {

class CurvilinearSkeleton;

// A skeleton line is a set of connected nodes such that the degree of each one is 2
struct SkeletonLine {
    std::vector<GraphNodeIndex> nodes; // The nodes;
    GraphNodeIndex extremums[2]; // The two extremum nodes (connected nodes that are not in the line)
    Vec3f extremumPositions[2]; // The positions of the extremum nodes gives the real straight line associated with the skeleton line

    void addNode(GraphNodeIndex n) {
        nodes.push_back(n);
    }
};

class SkeletonLineSet: std::vector<SkeletonLine> {
    void build(const CurvilinearSkeleton& skeleton);

    typedef std::vector<SkeletonLine> Base;
    std::vector<int> m_LineOf;
public:
    static const int NO_LINE = -1;
    static const int CURVATURE_NODE = -3;
    static const int COVERAGE_NODE = -4;

    SkeletonLineSet() {
    }

    SkeletonLineSet(const CurvilinearSkeleton& skeleton);

    int getLineOf(GraphNodeIndex node) const {
        return m_LineOf[node];
    }

    void setLineOf(GraphNodeIndex node, int line) {
        m_LineOf[node] = line;
    }

    void segmentWrtMaxballCurvature(const CurvilinearSkeleton& skeleton, float maxballScale = 1.f);

    void segmentWrtMaxballCoverage(const CurvilinearSkeleton& skeleton);

    void segmentWrtMaxballVariance(const CurvilinearSkeleton& skeleton);

    using Base::iterator;
    using Base::const_iterator;
    using Base::empty;
    using Base::size;
    using Base::operator [];
    using Base::begin;
    using Base::end;
};

/**
 * Compute the list of the indices of nodes that belongs to the maxball of a given node.
 */
GraphAdjacencyList computeMaxBallAdjacencyList(GraphNodeIndex node,
    const CurvilinearSkeleton& skeleton);

/**
 * Compute a graph MBG from the skeleton where the edge (i, j) is in MBG if j belongs to the maxball
 * of i.
 */
Graph computeMaxBallGraph(const CurvilinearSkeleton& skeleton);

GraphNodeIndex computeBiggestNode(const CurvilinearSkeleton& skeleton);

}
