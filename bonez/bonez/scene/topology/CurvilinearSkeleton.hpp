#pragma once

#include <bonez/utils/Graph.hpp>
#include <bonez/utils/Grid3D.hpp>

#include "bonez/scene/Intersection.hpp"
#include "bonez/scene/VoxelGrid.hpp"

#include "EmptySpaceVoxelMapping.hpp"

namespace BnZ {

class VoxelSpace;

class CurvilinearSkeleton {
public:
    struct Node {
        Vec3f P; //! Position of the node
        float maxball; //! Radius of the maxball centered in the node

        Node(Vec3f P, float maxball):
            P(P), maxball(maxball) {
        }

        Node() {
        }
    };

    typedef Grid3D<GraphNodeIndex> GridType;
    typedef std::vector<Node> NodeArray;

    uint32_t addNode(const Vec3f& position, float maxBallRadius) {
        m_Nodes.push_back({position, maxBallRadius});
        return m_Nodes.size() - 1;
    }

    void setGraph(Graph graph) {
        m_Graph = graph;
    }

    const Graph& getGraph() const {
        return m_Graph;
    }
    
    void setGrid(GridType grid) {
        m_Grid = std::move(grid);
    }

    void setGridToWorld(const Mat4f& gridToWorld, float gridToWorldScale) {
        m_LocalToWorld = gridToWorld;
        m_WorldToLocal = inverse(gridToWorld);
        m_fLocalToWorldScale = gridToWorldScale;
        m_fWorldToLocalScale = 1.f / m_fLocalToWorldScale;
    }

    void setWorldToGrid(const Mat4f& worldToGrid, float worldToGridScale) {
        m_WorldToLocal = worldToGrid;
        m_LocalToWorld = inverse(worldToGrid);
        m_fWorldToLocalScale = worldToGridScale;
        m_fLocalToWorldScale = 1.f / m_fWorldToLocalScale;
    }

    const Node& operator[](GraphNodeIndex idx) const {
        assert(idx != UNDEFINED_NODE);
        return m_Nodes[idx];
    }

    const Node& getNode(GraphNodeIndex idx) const {
        assert(idx != UNDEFINED_NODE);
        return m_Nodes[idx];
    }

    size_t size() const {
        return m_Nodes.size();
    }

    bool empty() const {
        return m_Nodes.empty();
    }

    const GridType& getGrid() const {
        return m_Grid;
    }

    GraphNodeIndex getNearestNode(const Vec3f& position) const {
        auto voxel = m_VoxelMapping.getVoxel(position, m_WorldToLocal);
        if(!m_Grid.contains(voxel)) {
            return UNDEFINED_NODE;
        }
        return m_Grid(voxel);
    }

    GraphNodeIndex getNearestNode(const Vec3f& position, const Vec3f& normal) const {
        auto voxel = m_VoxelMapping.getVoxel(position, normal, m_WorldToLocal, [&](const auto& voxel) {
            return this->isInEmptySpace(voxel);
        });
        if(!m_Grid.contains(voxel)) {
            return UNDEFINED_NODE;
        }
        return m_Grid(voxel);
    }

    GraphNodeIndex getNearestNode(const Vec3f& position, const Vec3f& normal, const Vec3f& incidentDirection) const {
        auto voxel = m_VoxelMapping.getVoxel(position, normal, incidentDirection, m_WorldToLocal, [&](const auto& voxel) {
            return this->isInEmptySpace(voxel);
        });
        if(!m_Grid.contains(voxel)) {
            return UNDEFINED_NODE;
        }
        return m_Grid(voxel);
    }

    GraphNodeIndex getNearestNode(const SurfacePoint& point) const {
        return getNearestNode(point.P, point.Ns);
    }

    GraphNodeIndex getNearestNode(const SurfacePoint& point, const Vec3f& incidentDirection) const {
        return getNearestNode(point.P, point.Ns, incidentDirection);
    }
    
    const GraphAdjacencyList& neighbours(GraphNodeIndex idx) const {
        assert(idx != UNDEFINED_NODE);
        return m_Graph[idx];
    }

    const Mat4f& getWorldToGridMatrix() const {
        return m_WorldToLocal;
    }

    const Mat4f& getGridToWorldMatrix() const {
        return m_LocalToWorld;
    }

    float getWorldToGridScale() const {
        return m_fWorldToLocalScale;
    }

    float getGridToWorldScale() const {
        return m_fLocalToWorldScale;
    }

    bool isInEmptySpace(const Vec3i& voxel) const {
        return m_Grid.contains(voxel) && m_Grid(voxel) != UNDEFINED_NODE;
    }

    void blur();

    void blurMaxballs();

private:
    Mat4f m_WorldToLocal;
    Mat4f m_LocalToWorld;
    float m_fWorldToLocalScale;
    float m_fLocalToWorldScale; // It is also the size of a voxel in world space

    NodeArray m_Nodes;
    Graph m_Graph;
    GridType m_Grid;

    EmptySpaceVoxelMapping m_VoxelMapping;
};

inline Mat4f getViewMatrix(const CurvilinearSkeleton::Node& node) {
    return getViewMatrix(node.P);
}

typedef std::vector<GraphNodeIndex> NodeVector;

/**
 * Compute a list of node which are local minimas in their neighbour for the maxball radius.
 */
NodeVector computeLocalMinimas(const CurvilinearSkeleton& skeleton);

CurvilinearSkeleton computeMaxballBasedSegmentedSkeleton(const CurvilinearSkeleton& skeleton);

}
