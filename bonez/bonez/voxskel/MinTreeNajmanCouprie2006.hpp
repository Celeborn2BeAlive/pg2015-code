#pragma once

#include <algorithm>
#include <bonez/utils/UnionFind.hpp>
#include <bonez/utils/Grid3D.hpp>
#include <bonez/sys/easyloggingpp.hpp>

namespace BnZ {

template<typename GridValueType>
class MinTreeNajmanCouprie2006 {
    struct Node {
        std::vector<size_t> m_ChildrenArray;
        GridValueType m_Level;

        Node() = default;

        Node(GridValueType level): m_Level(level) {
        }

        void addChild(size_t node) {
            m_ChildrenArray.push_back(node);
        }
    };
public:
    using value_type = GridValueType;

    MinTreeNajmanCouprie2006() = default;

    MinTreeNajmanCouprie2006(const Grid3D<GridValueType>& grid) {
        auto isGreaterThan = [&](auto a, auto b) {
            if(grid[a] > grid[b]) {
                return true;
            }
            if(grid[a] == grid[b]) {
                return a < b;
            }
            return false;
        };

        auto size = grid.size();
        std::vector<size_t> voxelSorting;
        for(auto i: range(size)) {
            voxelSorting.emplace_back(i);
            m_Nodes.emplace_back(grid[i]);
            m_LowestNode.emplace_back(i);
        }
        std::sort(begin(voxelSorting), end(voxelSorting), isGreaterThan);
        m_TreeUnionFind = UnionFind(size);
        m_NodeUnionFind = UnionFind(size);

        for(auto p: voxelSorting) {
            auto currentTree = m_TreeUnionFind.find(p);
            auto currentNode = m_NodeUnionFind.find(m_LowestNode[currentTree]);

            foreach6Neighbour(grid.resolution(), grid.coords(p), [&](const Vec3i& neighbourVoxel) {
                auto q = grid.offset(neighbourVoxel);
                if(isGreaterThan(q, p)) { // In that case, q has already been processed
                    auto adjTree = m_TreeUnionFind.find(q);
                    auto adjNode = m_NodeUnionFind.find(m_LowestNode[adjTree]);
                    if(currentNode != adjNode) {
                        if(m_Nodes[currentNode].m_Level == m_Nodes[adjNode].m_Level) {
                            currentNode = mergeNodes(adjNode, currentNode);
                        } else {
                            // We have m_Nodes[currentNode].level < m_Nodes[adjNode].level
                            m_Nodes[currentNode].addChild(adjNode);
                        }
                        currentTree = m_TreeUnionFind.link(adjTree, currentTree);
                        m_LowestNode[currentTree] = currentNode;
                    }
                }
            });
        }
        m_nRoot = m_LowestNode[m_TreeUnionFind.find(m_NodeUnionFind.find(0))];
        m_VoxelToNode = Grid3D<size_t>(grid.resolution());
        for(auto i: range(size)) {
            m_VoxelToNode[i] = m_NodeUnionFind.find(i);
        }
    }

    const Grid3D<size_t>& getVoxelToNodeMapping() const {
        return m_VoxelToNode;
    }

    size_t getRoot() const {
        return m_nRoot;
    }

    const std::vector<size_t>& getChildren(size_t node) const {
        return m_Nodes[node].m_ChildrenArray;
    }

    GridValueType getLevel(size_t node) const {
        return m_Nodes[node].m_Level;
    }

private:
    size_t mergeNodes(size_t node1, size_t node2) {
        auto tmpNode = m_NodeUnionFind.link(node1, node2);

        if(tmpNode == node2) {
            for(auto child: m_Nodes[node1].m_ChildrenArray) {
                m_Nodes[node2].addChild(child);
            }
        } else {
            for(auto child: m_Nodes[node2].m_ChildrenArray) {
                m_Nodes[node1].addChild(child);
            }
        }

        return tmpNode;
    }

    size_t m_nRoot;
    std::vector<Node> m_Nodes;
    UnionFind m_TreeUnionFind;
    UnionFind m_NodeUnionFind;
    Grid3D<size_t> m_VoxelToNode;
    std::vector<size_t> m_LowestNode;
};

}
