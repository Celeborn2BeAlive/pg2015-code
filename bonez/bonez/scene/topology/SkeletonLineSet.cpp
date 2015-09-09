#include "SkeletonLineSet.hpp"
#include "CurvilinearSkeleton.hpp"

#include "bonez/maths/maths.hpp"
#include "bonez/utils/itertools/itertools.hpp"

#include <iostream>

namespace BnZ {

SkeletonLineSet::SkeletonLineSet(const CurvilinearSkeleton& skeleton):
    m_LineOf(skeleton.size(), -1) {
    build(skeleton);
}

void SkeletonLineSet::build(const CurvilinearSkeleton& skeleton) {
    // Build the lines
    for(GraphNodeIndex i = 0; i < skeleton.size(); ++i) {
        if(skeleton.neighbours(i).size() != 2) {
            // The degree is not 2, the node doesn't belong to a line
            setLineOf(i, NO_LINE);

            for(auto neighbour: skeleton.neighbours(i)) {
                if(skeleton.neighbours(neighbour).size() == 2 &&
                        getLineOf(neighbour) == NO_LINE) {
                    int newLineIndex = size();
                    emplace_back();

                    SkeletonLine& line = back();
                    line.extremums[0] = i;
                    line.extremumPositions[0] = skeleton.getNode(i).P;

                    auto parent = i;
                    auto current = neighbour;
                    while(skeleton.neighbours(current).size() == 2) {
                        line.addNode(current);
                        setLineOf(current, newLineIndex);

                        if(skeleton.neighbours(current)[0] != parent) {
                            parent = current;
                            current = skeleton.neighbours(current)[0];
                        } else {
                            parent = current;
                            current = skeleton.neighbours(current)[1];
                        }
                    }

                    line.extremums[1] = current;
                    line.extremumPositions[1] = skeleton.getNode(current).P;
                }
            }
        }
    }
}

#ifdef GIVES_AN_INTERESTING_CLUSTERING

void SkeletonLineSet::segmentWrtMaxballCoverage(const Skeleton& skeleton) {
    // Cut the lines that are not straight wrt. the maxballs
    for(uint32_t i = 0; i < size();) {
        Vec3f u = (*this)[i].extremumPositions[0] - (*this)[i].extremumPositions[1];

        uint32_t maxDistanceNode = UNDEFINED_NODE;
        float maxDistance = 0;

        // Evaluate the curvature of each node
        for(uint32_t j = 0; j < (*this)[i].nodes.size(); ++j) {
            GraphNodeIndex node = (*this)[i].nodes[j];
            Vec3f P = skeleton.getNode(node).P;
            Vec3f v = P - (*this)[i].extremumPositions[0];
            float distanceToStraightLine = length(cross(v, u)) / length(u);
            if(sqr(distanceToStraightLine) > skeleton.getNode(node).maxball) {
                // If the distance to the line is greater than the radius of the maxball
                // then the line go outside the maxball and then potentially in the scene
                // geometry
                if(distanceToStraightLine > maxDistance) {
                    maxDistance = distanceToStraightLine;
                    maxDistanceNode = j;
                }
            }
        }

        // If the line is not straight it must be cut; we introduce a new cluster at the position
        // of the node of maximal curvature
        if(UNDEFINED_NODE != maxDistanceNode) {
            // The node that will be removed from the line
            GraphNodeIndex pivot = (*this)[i].nodes[maxDistanceNode];

            // Add a new line for nodes after the pivot
            int newLineIndex = size();
            emplace_back();

            back().extremums[0] = (*this)[i].nodes[maxDistanceNode];
            back().extremumPositions[0] = skeleton.getNode(back().extremums[0]).P;
            back().extremums[1] = (*this)[i].extremums[1];
            back().extremumPositions[1] = (*this)[i].extremumPositions[1];
            for(uint32_t k = maxDistanceNode + 1; k < (*this)[i].nodes.size(); ++k) {
                back().addNode((*this)[i].nodes[k]);
                setLineOf((*this)[i].nodes[k], newLineIndex);
            }

            // Change second extremum position for the new line before the pivot and remove nodes after the pivot
            (*this)[i].extremums[1] = (*this)[i].nodes[maxDistanceNode];
            (*this)[i].extremumPositions[1] = skeleton.getNode(back().extremums[1]).P;
            (*this)[i].nodes.resize(maxDistanceNode);

            setLineOf(pivot, CURVATURE_NODE);
        } else {
            ++i;
        }
    }
}

#endif

void SkeletonLineSet::segmentWrtMaxballCurvature(const CurvilinearSkeleton& skeleton, float maxballScale) {
    // Cut the lines that are not straight wrt. the maxballs
    for(uint32_t i = 0; i < size(); ) {
        Vec3f u = (*this)[i].extremumPositions[0] - (*this)[i].extremumPositions[1];

        bool isStraight = true;

        for(uint32_t j = 0; j < (*this)[i].nodes.size(); ++j) {
            GraphNodeIndex node = (*this)[i].nodes[j];
            Vec3f P = skeleton.getNode(node).P;
            Vec3f v = P - (*this)[i].extremumPositions[0];
            float distanceToStraightLine;

            if((*this)[i].extremums[0] != (*this)[i].extremums[1]) {
                distanceToStraightLine = length(cross(v, u)) / length(u);
            } else {
                distanceToStraightLine = distance((*this)[i].extremumPositions[0],
                        (*this)[i].extremumPositions[1]);
            }

            float maxballCurvature = distanceToStraightLine / (maxballScale * skeleton.getNode(node).maxball);
            if(maxballCurvature > 1.f) {
                isStraight = false;
                break;
            }
        }

        if(!isStraight) {
            uint32_t maxCurvatureNode = std::numeric_limits<uint32_t>::max();
            float maxDistance = 0;

            // Evaluate the curvature of each node
            for(uint32_t j = 0; j < (*this)[i].nodes.size(); ++j) {
                GraphNodeIndex node = (*this)[i].nodes[j];
                Vec3f P = skeleton.getNode(node).P;
                Vec3f v = P - (*this)[i].extremumPositions[0];

                float distanceToStraightLine;

                if((*this)[i].extremums[0] != (*this)[i].extremums[1]) {
                    distanceToStraightLine = length(cross(v, u)) / length(u);
                } else {
                    distanceToStraightLine = distance((*this)[i].extremumPositions[0],
                            (*this)[i].extremumPositions[1]);
                }

                if(distanceToStraightLine > maxDistance) {
                    maxDistance = distanceToStraightLine;
                    maxCurvatureNode = j;
                }
            }

            // The node that will be removed from the line
            GraphNodeIndex pivot = (*this)[i].nodes[maxCurvatureNode];

            // Add a new line for nodes after the pivot
            int newLineIndex = size();
            emplace_back();

            back().extremums[0] = pivot;
            back().extremumPositions[0] = skeleton.getNode(pivot).P;
            back().extremums[1] = (*this)[i].extremums[1];
            back().extremumPositions[1] = (*this)[i].extremumPositions[1];
            for(uint32_t k = maxCurvatureNode + 1; k < (*this)[i].nodes.size(); ++k) {
                back().addNode((*this)[i].nodes[k]);
                setLineOf((*this)[i].nodes[k], newLineIndex);
            }

            // Change second extremum position for the new line before the pivot and remove nodes after the pivot
            (*this)[i].extremums[1] = pivot;
            (*this)[i].extremumPositions[1] = skeleton.getNode(pivot).P;
            (*this)[i].nodes.resize(maxCurvatureNode);

            setLineOf(pivot, CURVATURE_NODE);

        } else {
            ++i;
        }

        /*
        uint32_t maxCurvatureNode = std::numeric_limits<uint32_t>::max();
        float maxCurvature = 0;

        // Evaluate the curvature of each node
        for(uint32_t j = 0; j < (*this)[i].nodes.size(); ++j) {
            GraphNodeIndex node = (*this)[i].nodes[j];
            Vec3f P = skeleton.getNode(node).P;
            Vec3f v = P - (*this)[i].extremumPositions[0];
            float distanceToStraightLine = length(cross(v, u)) / length(u);
            float maxballCurvature = distanceToStraightLine / (maxballScale * skeleton.getNode(node).maxball);
            if(maxballCurvature > 1.f) {
                // If the distance to the line is greater than the radius of the maxball
                // then the line go outside the maxball and then potentially in the scene
                // geometry
                if(distanceToStraightLine > maxCurvature) {
                    maxCurvature = distanceToStraightLine;
                    maxCurvatureNode = j;
                }
            }
        }

        // If the line is not straight it must be cut; we introduce a new cluster at the position
        // of the node of maximal curvature
        if(std::numeric_limits<uint32_t>::max() != maxCurvatureNode) {
            // The node that will be removed from the line
            GraphNodeIndex pivot = (*this)[i].nodes[maxCurvatureNode];

            // Add a new line for nodes after the pivot
            int newLineIndex = size();
            emplace_back();

            back().extremums[0] = pivot;
            back().extremumPositions[0] = skeleton.getNode(pivot).P;
            back().extremums[1] = (*this)[i].extremums[1];
            back().extremumPositions[1] = (*this)[i].extremumPositions[1];
            for(uint32_t k = maxCurvatureNode + 1; k < (*this)[i].nodes.size(); ++k) {
                back().addNode((*this)[i].nodes[k]);
                setLineOf((*this)[i].nodes[k], newLineIndex);
            }

            // Change second extremum position for the new line before the pivot and remove nodes after the pivot
            (*this)[i].extremums[1] = pivot;
            (*this)[i].extremumPositions[1] = skeleton.getNode(pivot).P;
            (*this)[i].nodes.resize(maxCurvatureNode);

            setLineOf(pivot, CURVATURE_NODE);
        } else {
            ++i;
        }*/
    }
}

void SkeletonLineSet::segmentWrtMaxballCoverage(const CurvilinearSkeleton& skeleton) {
    // Add nodes to improve coverage of edges by tge maxballs
    for(uint32_t i = 0; i < size();) {
        Vec3f center = ((*this)[i].extremumPositions[0] + (*this)[i].extremumPositions[1]) * 0.5f;

        float extremum1Maxball = skeleton.getNode((*this)[i].extremums[0]).maxball;
        float extremum1MaxballSquared = sqr(extremum1Maxball);
        float extremum2Maxball = skeleton.getNode((*this)[i].extremums[1]).maxball;
        float extremum2MaxballSquared = sqr(extremum2Maxball);

        uint32_t pivotIndex = std::numeric_limits<uint32_t>::max();
        float distanceToCenter = std::numeric_limits<float>::max();;

        if(true || (getLineOf((*this)[i].extremums[0]) == NO_LINE && getLineOf((*this)[i].extremums[1]) == NO_LINE)) {
            for(uint32_t j = 0; j < (*this)[i].nodes.size(); ++j) {
                GraphNodeIndex node = (*this)[i].nodes[j];
                Vec3f P = skeleton.getNode(node).P;
                //float maxball = sqr(skeleton.getNode(node).maxball);
                float distanceSquaredToExtremum1 = distanceSquared(P, (*this)[i].extremumPositions[0]);
                float distanceSquaredToExtremum2 = distanceSquared(P, (*this)[i].extremumPositions[1]);
                if(distanceSquaredToExtremum1 > extremum1MaxballSquared &&
                        distanceSquaredToExtremum2 > extremum2MaxballSquared) {
                    float d = distanceSquared(P, center);
                    if(d < distanceToCenter) {
                        distanceToCenter = d;
                        pivotIndex = j;
                    }
                }
            }
        }

        // If a pivot has been found, cut the line and create a new cluster at the pivot position
        if(std::numeric_limits<uint32_t>::max() != pivotIndex) {
            // The node that will be removed from the line
            GraphNodeIndex pivot = (*this)[i].nodes[pivotIndex];

            auto pivotNode = skeleton.getNode(pivot);

            float nodeMaxballVariance1
                    = abs(1 - extremum1Maxball / pivotNode.maxball);

            if(nodeMaxballVariance1 < 0.2) {
                ++i;
                continue;
            }

            float nodeMaxballVariance2
                    = abs(1 - extremum2Maxball / pivotNode.maxball);

            if(nodeMaxballVariance2 < 0.2) {
                ++i;
                continue;
            }

            // Add a new line for nodes after the pivot
            int newLineIndex = size();
            emplace_back();

            back().extremums[0] = pivot;
            back().extremumPositions[0] = skeleton.getNode(pivot).P;
            back().extremums[1] = (*this)[i].extremums[1];
            back().extremumPositions[1] = (*this)[i].extremumPositions[1];
            for(uint32_t k = pivotIndex + 1; k < (*this)[i].nodes.size(); ++k) {
                back().addNode((*this)[i].nodes[k]);
                setLineOf((*this)[i].nodes[k], newLineIndex);
            }

            // Change second extremum position for the new line before the pivot and remove nodes after the pivot
            (*this)[i].extremums[1] = pivot;
            (*this)[i].extremumPositions[1] = skeleton.getNode(pivot).P;
            (*this)[i].nodes.resize(pivotIndex);

            setLineOf(pivot, COVERAGE_NODE);
        } else {
            ++i;
        }
    }
}

void SkeletonLineSet::segmentWrtMaxballVariance(const CurvilinearSkeleton& skeleton) {
    // Add nodes to improve coverage of edges by tge maxballs
    for(uint32_t i = 0; i < size();) {
        float sumMaxballs = 0;
        for(auto node: (*this)[i].nodes) {
            sumMaxballs += skeleton.getNode(node).maxball;
        }
        float meanMaxballs = sumMaxballs / (*this)[i].nodes.size();
        float maxVariance = 0;
        uint32_t pivotIndex = std::numeric_limits<uint32_t>::max();
        GraphNodeIndex pivot = UNDEFINED_NODE;
        for(auto node: index((*this)[i].nodes)) {
            float variance = sqr(skeleton.getNode(node.second).maxball - meanMaxballs);
            if(variance > maxVariance) {
                maxVariance = variance;
                pivotIndex = node.first;
                pivot = node.second;
            }
        }

        float stdDeviation = sqrt(maxVariance);

        if(pivot != UNDEFINED_NODE &&
                stdDeviation > 0.5 * meanMaxballs) {
            float extremum1Maxball = skeleton.getNode((*this)[i].extremums[0]).maxball;
            float extremum1MaxballSquared = sqr(extremum1Maxball);
            float extremum2Maxball = skeleton.getNode((*this)[i].extremums[1]).maxball;
            float extremum2MaxballSquared = sqr(extremum2Maxball);

            Vec3f P = skeleton.getNode(pivot).P;
            float distanceSquaredToExtremum1 = distanceSquared(P, (*this)[i].extremumPositions[0]);
            float distanceSquaredToExtremum2 = distanceSquared(P, (*this)[i].extremumPositions[1]);

            if(distanceSquaredToExtremum1 > extremum1MaxballSquared &&
               distanceSquaredToExtremum2 > extremum2MaxballSquared) {
                // Add a new line for nodes after the pivot
                int newLineIndex = size();
                emplace_back();

                back().extremums[0] = pivot;
                back().extremumPositions[0] = skeleton.getNode(pivot).P;
                back().extremums[1] = (*this)[i].extremums[1];
                back().extremumPositions[1] = (*this)[i].extremumPositions[1];
                for(uint32_t k = pivotIndex + 1; k < (*this)[i].nodes.size(); ++k) {
                    back().addNode((*this)[i].nodes[k]);
                    setLineOf((*this)[i].nodes[k], newLineIndex);
                }

                // Change second extremum position for the new line before the pivot and remove nodes after the pivot
                (*this)[i].extremums[1] = pivot;
                (*this)[i].extremumPositions[1] = skeleton.getNode(pivot).P;
                (*this)[i].nodes.resize(pivotIndex);

                setLineOf(pivot, NO_LINE);
            } else {
                ++i;
            }
        } else {
            ++i;
        }
    }
}

static void recursiveComputeMaxBallNeighbours(GraphNodeIndex source, GraphNodeIndex current,
    const CurvilinearSkeleton& skeleton, std::vector<bool>& visitMap, GraphAdjacencyList& neighbours) {

    if(visitMap[current]) {
        return;
    }

    visitMap[current] = true;

    for(GraphNodeIndex directNeighbour: skeleton.neighbours(current)) {
        Vec3f v = skeleton.getNode(directNeighbour).P - skeleton.getNode(source).P;
        if(length(v) < skeleton.getNode(source).maxball) {
            neighbours.push_back(directNeighbour);
            recursiveComputeMaxBallNeighbours(source, directNeighbour, skeleton, visitMap, neighbours);
        }
    }
}

GraphAdjacencyList computeMaxBallAdjacencyList(GraphNodeIndex node,
    const CurvilinearSkeleton& skeleton) {
    GraphAdjacencyList list;

    //recursiveComputeMaxBallNeighbours(node, node, skeleton, visitMap, list);

    for(GraphNodeIndex other: range(skeleton.size())) {
        Vec3f v = skeleton.getNode(other).P - skeleton.getNode(node).P;
        if(length(v) < skeleton.getNode(node).maxball) {
            list.push_back(other);
        }
    }

    return list;
}

Graph computeMaxBallGraph(const CurvilinearSkeleton& skeleton) {
    Graph graph(skeleton.size());

    std::vector<bool> visitMap(skeleton.size(), false);

    for(GraphNodeIndex idx = 0, end = skeleton.size(); idx != end; ++idx) {
        recursiveComputeMaxBallNeighbours(idx, idx, skeleton, visitMap, graph[idx]);

        for(GraphNodeIndex n: graph[idx]) {
            visitMap[n] = false;
        }
    }

    return graph;
}

GraphNodeIndex computeBiggestNode(const CurvilinearSkeleton& skeleton) {
    float max = 0.f;
    GraphNodeIndex current = UNDEFINED_NODE;
    for(GraphNodeIndex node: range(skeleton.size())) {
        if(skeleton.getNode(node).maxball > max) {
            max = skeleton.getNode(node).maxball;
            current = node;
        }
    }
    return current;
}

}
