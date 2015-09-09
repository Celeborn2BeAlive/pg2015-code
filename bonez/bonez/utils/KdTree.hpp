#pragma once

#include <vector>
#include <cinttypes>
#include <algorithm>

#include <bonez/common.hpp>
#include <bonez/maths/maths.hpp>
#include <bonez/maths/BBox.hpp>

namespace BnZ {

/**
 * @brief The KdNode struct represents a node of a KdTree.
 */
struct KdNode {
    static const uint32_t NO_NODE = uint32_t(-1);//std::numeric_limits<uint32_t>::max(); visual studo numeric_limits::max() is not constant expr
    static const uint32_t NO_RIGHT_CHILD = (1 << 29) - 1;
    static const uint32_t NO_SPLIT_AXIS = 3;

    //! The coordinate of the node along the split axis
    float m_fSplitPosition;
    //! The split axis in [0,3[
    uint32_t m_nSplitAxis: 2;
    //! A boolean indicating if the node has a left child
    bool m_bHasLeftChild: 1;
    //! The index of the right child of the node in the KdTree array
    uint32_t m_nRightChildIndex: 29;

    /**
     * @brief setAsInnerNode set the members to represent an inner node.
     * @param splitPosition The coordinate of the node along the split axis
     * @param splitAxis The split axis in [0,3[
     *
     * @remark The members m_bHasLeftChild and m_nRightChildIndex must be set by the caller
     * after calling this method.
     */
    void setAsInnerNode(float splitPosition, uint32_t splitAxis) {
        m_fSplitPosition = splitPosition;
        m_nSplitAxis = splitAxis;
        m_nRightChildIndex = NO_RIGHT_CHILD;
        m_bHasLeftChild = false;
    }

    /**
     * @brief setAsLeaf set the members to represent a leaf.
     */
    void setAsLeaf() {
        m_nSplitAxis = NO_SPLIT_AXIS;
        m_nRightChildIndex = NO_RIGHT_CHILD;
        m_bHasLeftChild = false;
    }
};

class KdTree
{
public:
    bool empty() const {
        return m_Nodes.empty();
    }

    size_t size() const {
        return m_Nodes.size();
    }

    /**
     * @brief build the KdTree.
     * @param count The number of elements to put in the KdTree.
     * @param getPosition A functor such that getPosition(i) is the 3D position of the element i.
     * @param isValid A functor such that isValid(i) returns true if the element i must be included in the KdTree
     */
    template<typename PositionFunctor, typename IsValidFunctor>
    void build(size_t count, PositionFunctor getPosition, IsValidFunctor isValid) {
        clear();

        // Extract valid indices
        std::vector<uint32_t> indices;
        indices.reserve(count);
        for(uint32_t i = 0; i < count; ++i) {
            if(isValid(i)) {
                indices.push_back(i);
            }
        }

        m_Nodes.resize(indices.size());
        m_NodesData.resize(indices.size());
        uint32_t nextFreeNode = 1;

        recursiveBuild(0, 0, indices.size(), indices.data(), getPosition, nextFreeNode);
    }

    template<typename PositionFunctor>
    void build(size_t count, PositionFunctor getPosition) {
        build(count, getPosition, [](uint32_t i) { return true; });
    }

    /**
     * @brief Search for all the elements inside a ball.
     * @param point The center of the ball.
     * @param maxDistanceSquared The squared radius of the ball.
     * @param f A functor with sign (uint32_t idx, Vec3f position, float distSquared, float& maxDistSquared)
     * which is called for each element in the ball. The functor can modify the maximum distance squared.
     */
    template<typename ProcessFunctor>
    void search(const Vec3f& point, float maxDistanceSquared, ProcessFunctor process) const {
        if(empty()) {
            return;
        }
        recursiveSearch(0, point, maxDistanceSquared, process);
    }

    /**
     * @brief Search the nearest neighbor i of a given point that match the predicate predicate(i)
     */
    template<typename Predicate>
    uint32_t searchNearestNeighbour(const Vec3f& point, float& distSquared, Predicate predicate) const {
        distSquared = std::numeric_limits<float>::infinity();
        if(empty()) {
            return std::numeric_limits<uint32_t>::max();
        }
        uint32_t nodeIndex = recursiveSearchNearestNeighbour(0, point, distSquared, predicate);
        if(nodeIndex == KdNode::NO_NODE) {
            return std::numeric_limits<uint32_t>::max();
        }
        return m_NodesData[nodeIndex].m_nIndex;
    }

    uint32_t searchNearestNeighbour(const Vec3f& point, float& distSquared) const {
        return searchNearestNeighbour(point, distSquared,
                                      [](uint32_t idx) { return true; });
    }

    /**
     * @brief Search the nearest neighbors of a given point that match the predicate predicate(i)
     */
    template<typename Predicate, typename ProcessFunctor>
    void searchKNearestNeighbours(const Vec3f& point, size_t K, Predicate predicate,
                                 ProcessFunctor process) const {
        if(empty() || !K) {
            return;
        }
        std::vector<std::pair<uint32_t, float>> heap;
        float distSquared = std::numeric_limits<float>::infinity();
        recursiveSearchKNearestNeighbours(0, point, K, predicate, distSquared, heap);

        for(const auto& value: heap) {
            process(m_NodesData[value.first].m_nIndex, m_NodesData[value.first].m_Position,
                    value.second);
        }
    }

    template<typename ProcessFunctor>
    void searchKNearestNeighbours(const Vec3f& point, size_t K,
                                 ProcessFunctor process) const {
        searchKNearestNeighbours(point, K, [](uint32_t idx) { return true; }, process);
    }

    /**
     * @brief traverse the KdTree in a depth first order.
     * @param f A functor with sign (uint32_t parent, uint32_t child) which is called for each
     * traversed edge in the order of traversal.
     */
    template<typename Functor>
    void depthFirstTraversal(Functor f) const {
        if(empty()) {
            return;
        }
        recursiveDepthFirstTraversal(0, f);
    }

    void clear() {
        m_Nodes.clear();
        m_NodesData.clear();
    }
private:
    template<typename PositionFunctor>
    void recursiveBuild(uint32_t nodeIndex, uint32_t start, uint32_t end, uint32_t* indices,
                        PositionFunctor getPosition, uint32_t& nextFreeNode) {
        if(start + 1 == end) {
            // One node to process, it's a  leaf
            m_Nodes[nodeIndex].setAsLeaf();

            m_NodesData[nodeIndex].m_nIndex = indices[start];
            m_NodesData[nodeIndex].m_Position = getPosition(indices[start]);

            return;
        }

        // Compute the bounding box of the data
        BBox3f bound;
        for(uint32_t i = start; i != end; ++i) {
            bound += getPosition(indices[i]);
        }
        // The split axis is the one with maximal extent for the data
        uint32_t splitAxis = maxComponent(abs(bound.size()));
        uint32_t splitIndex = (start + end) / 2;
        // Reorganize the pointers such that the middle element is the middle element on the split axis
        std::nth_element(indices + start, indices + splitIndex, indices + end,
                         [splitAxis, &getPosition](uint32_t lhs, uint32_t rhs) -> bool {
                            float v1 = getPosition(lhs)[splitAxis];
                            float v2 = getPosition(rhs)[splitAxis];
                            return v1 == v2 ? lhs < rhs : v1 < v2;
                         });
        float splitPosition = getPosition(indices[splitIndex])[splitAxis];
        m_Nodes[nodeIndex].setAsInnerNode(splitPosition, splitAxis);
        m_NodesData[nodeIndex].m_nIndex = indices[splitIndex];
        m_NodesData[nodeIndex].m_Position = getPosition(indices[splitIndex]);

        // Build the left subtree
        if(start < splitIndex) {
            m_Nodes[nodeIndex].m_bHasLeftChild = true;
            uint32_t childIndex = nextFreeNode++;
            recursiveBuild(childIndex, start, splitIndex, indices, getPosition, nextFreeNode);
        }

        // Build the right subtree
        if(splitIndex + 1 < end) {
            m_Nodes[nodeIndex].m_nRightChildIndex = nextFreeNode++;
            recursiveBuild(m_Nodes[nodeIndex].m_nRightChildIndex, splitIndex + 1, end, indices, getPosition, nextFreeNode);
        }
    }

    template<typename ProcessFunctor>
    void recursiveSearch(uint32_t nodeIndex, const Vec3f& point, float& maxDistanceSquared, ProcessFunctor process) const {
        const KdNode& node = m_Nodes[nodeIndex];
        uint32_t axis = node.m_nSplitAxis;
        if(axis != 3) {
            float distSquared = sqr(point[axis] - node.m_fSplitPosition);
            if(point[axis] <= node.m_fSplitPosition) {
                if(node.m_bHasLeftChild) {
                    recursiveSearch(nodeIndex + 1, point, maxDistanceSquared, process);
                }
                if(distSquared < maxDistanceSquared && node.m_nRightChildIndex < m_Nodes.size()) {
                    recursiveSearch(node.m_nRightChildIndex, point, maxDistanceSquared, process);
                }
            } else {
                if(node.m_nRightChildIndex < m_Nodes.size()) {
                    recursiveSearch(node.m_nRightChildIndex, point, maxDistanceSquared, process);
                }
                if(distSquared < maxDistanceSquared && node.m_bHasLeftChild) {
                    recursiveSearch(nodeIndex + 1, point, maxDistanceSquared, process);
                }
            }
        }
        float distSquared = distanceSquared(m_NodesData[nodeIndex].m_Position, point);
        if(distSquared < maxDistanceSquared) {
            process(m_NodesData[nodeIndex].m_nIndex, m_NodesData[nodeIndex].m_Position, distSquared, maxDistanceSquared);
        }
    }

    template<typename Functor>
    void recursiveDepthFirstTraversal(uint32_t nodeIndex, Functor f) const {
        const KdNode& node = m_Nodes[nodeIndex];

        if(node.m_bHasLeftChild) {
            f(m_NodesData[nodeIndex].m_nIndex, m_NodesData[nodeIndex + 1].m_nIndex);
            recursiveDepthFirstSearch(nodeIndex + 1, f);
        }

        if(node.m_nRightChildIndex < m_Nodes.size()) {
            f(m_NodesData[nodeIndex].m_nIndex, m_NodesData[node.m_nRightChildIndex].m_nIndex);
            recursiveDepthFirstSearch(node.m_nRightChildIndex, f);
        }
    }

    template<typename Predicate>
    uint32_t recursiveSearchNearestNeighbour(uint32_t nodeIndex, const Vec3f& point,
                                             float& distSquared, Predicate predicate) const {
        auto node = m_Nodes[nodeIndex];
        uint32_t index = m_NodesData[nodeIndex].m_nIndex;

        float candidateDistSquared = distanceSquared(point, m_NodesData[nodeIndex].m_Position);

        // If the node is a leaf, end of the recursion
        if(node.m_nSplitAxis == 3) {
            if(candidateDistSquared < distSquared && predicate(index)) {
                distSquared = candidateDistSquared;
                return nodeIndex;
            }
            return KdNode::NO_NODE;
        }

        // If the node is inner, find a neighbour in the side of the point
        uint32_t currentBestMatch = KdNode::NO_NODE;
        uint8_t axis = node.m_nSplitAxis;

        bool isAtLeft = (point[axis] <= node.m_fSplitPosition);

        // Left side
        if(isAtLeft && node.m_bHasLeftChild) {
            currentBestMatch = recursiveSearchNearestNeighbour(nodeIndex + 1, point, distSquared, predicate);
        }
        // Right side
        else if(!isAtLeft && node.m_nRightChildIndex < m_Nodes.size()) {
            currentBestMatch = recursiveSearchNearestNeighbour(node.m_nRightChildIndex, point, distSquared, predicate);
        }

        // Test the current node against the actual best match
        if(candidateDistSquared < distSquared && predicate(index)) {
            currentBestMatch = nodeIndex;
            distSquared = candidateDistSquared;
        }

        uint32_t candidate = KdNode::NO_NODE;
        float axisDistSquared = sqr(point[axis] - node.m_fSplitPosition);

        // If the separation axis is closer to the point that the actual best match
        // we must search the other side
        if(axisDistSquared < distSquared) {
            if(isAtLeft && node.m_nRightChildIndex < m_Nodes.size()) {
                candidate = recursiveSearchNearestNeighbour(node.m_nRightChildIndex, point, distSquared, predicate);
            }
            else if(!isAtLeft && node.m_bHasLeftChild) {
                candidate = recursiveSearchNearestNeighbour(nodeIndex + 1, point, distSquared, predicate);
            }
        }

        if(candidate != KdNode::NO_NODE) {
            currentBestMatch = candidate;
        }

        return currentBestMatch;
    }

    static bool compare(const std::pair<uint32_t, float>& node1,
                        const std::pair<uint32_t, float>& node2) {
        return node1.second < node2.second;
    }

    template<typename Predicate>
    void recursiveSearchKNearestNeighbours(uint32_t nodeIndex, const Vec3f& point, size_t K, Predicate predicate,
                                           float& distSquared, std::vector<std::pair<uint32_t, float>>& heap) const {
        auto node = m_Nodes[nodeIndex];
        uint32_t index = m_NodesData[nodeIndex].m_nIndex;

        float candidateDistSquared = distanceSquared(point, m_NodesData[nodeIndex].m_Position);

        // If the node is a leaf, end of the recursion
        if(node.m_nSplitAxis == 3) {
            if((heap.size() < K || candidateDistSquared < distSquared) && predicate(index)) {
                heap.push_back(std::make_pair(nodeIndex, candidateDistSquared));
                std::push_heap(heap.begin(), heap.end(), compare);

                if(heap.size() > K) {
                    std::pop_heap(heap.begin(), heap.end(), compare);
                    heap.pop_back();
                }

                distSquared = heap.front().second;
            }
            return;
        }

        uint8_t axis = node.m_nSplitAxis;

        bool isAtLeft = (point[axis] <= node.m_fSplitPosition);

        // Left side
        if(isAtLeft && node.m_bHasLeftChild) {
            recursiveSearchKNearestNeighbours(nodeIndex + 1, point, K, predicate, distSquared, heap);
        }
        // Right side
        else if(!isAtLeft && node.m_nRightChildIndex < m_Nodes.size()) {
            recursiveSearchKNearestNeighbours(node.m_nRightChildIndex, point, K, predicate, distSquared, heap);
        }

        // Test the current node against the actual best match
        if((heap.size() < K || candidateDistSquared < distSquared) && predicate(index)) {
            heap.push_back(std::make_pair(nodeIndex, candidateDistSquared));
            std::push_heap(heap.begin(), heap.end(), compare);

            if(heap.size() > K) {
                std::pop_heap(heap.begin(), heap.end(), compare);
                heap.pop_back();
            }

            distSquared = heap.front().second;
        }

        float axisDistSquared = sqr(point[axis] - node.m_fSplitPosition);

        // If the separation axis is closer to the point than the actual best match
        // we must search the other side
        if(heap.size() < K || axisDistSquared < distSquared) {
            if(isAtLeft && node.m_nRightChildIndex < m_Nodes.size()) {
                recursiveSearchKNearestNeighbours(node.m_nRightChildIndex, point, K, predicate, distSquared, heap);
            }
            else if(!isAtLeft && node.m_bHasLeftChild) {
                recursiveSearchKNearestNeighbours(nodeIndex + 1, point, K, predicate, distSquared, heap);
            }
        }
    }

    struct NodeData {
        uint32_t m_nIndex;
        Vec3f m_Position;
    };

    std::vector<KdNode> m_Nodes;
    std::vector<NodeData> m_NodesData;
};

}
