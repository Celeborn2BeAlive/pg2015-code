#include <iostream>
#include <stack>
#include <algorithm>
#include "discrete_functions.hpp"

#include "CubicalComplex3D.hpp"

namespace BnZ {

uint32_t CubicalComplex3D::recursiveDepthFirstTraversal(int x, int y, int z, std::vector<bool>& visited) const {
    std::stack<glm::ivec3> stack;

    glm::ivec3 parent = glm::ivec3(x, y, z);

    auto idx = offset(x, y, z);
    if(!contains(x, y, z) || visited[idx] || !(*this)[idx].exists()) {
        return 0;
    }

    visited[idx] = true;
    stack.push(parent);

    auto f = [&](int x, int y, int z, int8_t element) {
        auto voxel = glm::ivec3(x, y, z);
        if(contains(voxel)) {
            auto idx = offset(voxel);
            if(!visited[idx] && (*this)[idx].containsSome(element)) {
                visited[idx] = true;
                stack.push(voxel);
            }
        }
    };

    while(!stack.empty()) {
        parent = stack.top();
        stack.pop();
        x = parent.x;
        y = parent.y;
        z = parent.z;
        idx = offset(x, y, z);

        if((*this)[idx].containsSome(CC3DFaceBits::POINT)) {
            f(x - 1, y, z, CC3DFaceBits::XEDGE);
            f(x, y, z - 1, CC3DFaceBits::ZEDGE);
            f(x, y - 1, z, CC3DFaceBits::YEDGE);
            f(x - 1, y, z - 1, CC3DFaceBits::XZFACE);
            f(x - 1, y - 1, z, CC3DFaceBits::XYFACE);
            f(x, y - 1, z - 1, CC3DFaceBits::YZFACE);
            f(x - 1, y - 1, z - 1, CC3DFaceBits::CUBE);
        }

        if((*this)[idx].containsSome(CC3DFaceBits::XEDGE)) {
            f(x + 1, y, z, CC3DFaceBits::POINT);
            f(x + 1, y, z - 1, CC3DFaceBits::ZEDGE);
            f(x + 1, y - 1, z, CC3DFaceBits::YEDGE);
            f(x + 1, y - 1, z - 1, CC3DFaceBits::YZFACE);
        }

        if((*this)[idx].containsSome(CC3DFaceBits::YEDGE)) {
            f(x, y + 1, z, CC3DFaceBits::POINT);
            f(x - 1, y + 1, z, CC3DFaceBits::XEDGE);
            f(x, y + 1, z - 1, CC3DFaceBits::ZEDGE);
            f(x - 1, y + 1, z - 1, CC3DFaceBits::XZFACE);
        }

        if((*this)[idx].containsSome(CC3DFaceBits::ZEDGE)) {
            f(x, y, z + 1, CC3DFaceBits::POINT);
            f(x - 1, y, z + 1, CC3DFaceBits::XEDGE);
            f(x, y - 1, z + 1, CC3DFaceBits::YEDGE);
            f(x - 1, y - 1, z + 1, CC3DFaceBits::XYFACE);
        }

        if((*this)[idx].containsSome(CC3DFaceBits::XYFACE)) {
            f(x + 1, y + 1, z, CC3DFaceBits::POINT);
            f(x + 1, y + 1, z - 1, CC3DFaceBits::ZEDGE);
        }

        if((*this)[idx].containsSome(CC3DFaceBits::XZFACE)) {
            f(x + 1, y, z + 1, CC3DFaceBits::POINT);
            f(x + 1, y - 1, z + 1, CC3DFaceBits::YEDGE);
        }

        if((*this)[idx].containsSome(CC3DFaceBits::YZFACE)) {
            f(x, y + 1, z + 1,CC3DFaceBits:: POINT);
            f(x - 1, y + 1, z + 1, CC3DFaceBits::XEDGE);
        }

        if((*this)[idx].containsSome(CC3DFaceBits::CUBE)) {
            f(x + 1, y + 1, z + 1, CC3DFaceBits::POINT);
        }
    }

    return 1;
}

uint32_t CubicalComplex3D::computeConnexityNumber() const {
    std::vector<bool> visited(size(), false);

    uint32_t connexityNumber = 0;

    forEach([&](int x, int y, int z, const value_type& face) {
        connexityNumber += this->recursiveDepthFirstTraversal(x, y, z, visited);
    });

    return connexityNumber;
}

int CubicalComplex3D::computeEulerCharacteristic() const {
    int ki = 0;

    for(const auto& face: *this) {
        ki += (int(face.containsSome(CC3DFaceBits::POINT))
               - (int(face.containsSome(CC3DFaceBits::XEDGE)) + int(face.containsSome(CC3DFaceBits::YEDGE)) + int(face.containsSome(CC3DFaceBits::ZEDGE)))
               + (int(face.containsSome(CC3DFaceBits::XYFACE)) + int(face.containsSome(CC3DFaceBits::XZFACE)) + int(face.containsSome(CC3DFaceBits::YZFACE)))
               - int(face.containsSome(CC3DFaceBits::CUBE)));
    }

    return ki;
}

bool CubicalComplex3D::isFreeXNEG(int x, int y, int z) const{
    return isFreeXNEG1D(x, y, z) || isFreeXNEG2D(x, y, z) || isFreeXNEG3D(x, y, z);
}

bool CubicalComplex3D::isFreeXNEG3D(int x, int y, int z) const{
    return x > 0 &&
            (*this)(x - 1, y, z).containsSome(CC3DFaceBits::CUBE) &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::CUBE);
}

bool CubicalComplex3D::isFreeXNEG2D(int x, int y, int z) const{
    return isFreeXNEG2DXY(x,y,z) || isFreeXNEG2DXZ(x,y,z);
}

bool CubicalComplex3D::isFreeXNEG2DXY(int x, int y, int z) const {
    return x > 0 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::YZFACE | CC3DFaceBits::XYFACE) &&
            (*this)(x - 1, y, z).containsSome(CC3DFaceBits::XYFACE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::YZFACE));
}

bool CubicalComplex3D::isFreeXNEG2DXZ(int x, int y, int z) const{
    return x > 0 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::YZFACE | CC3DFaceBits::XZFACE) &&
            (*this)(x - 1, y, z).containsSome(CC3DFaceBits::XZFACE) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YZFACE));
}

bool CubicalComplex3D::isFreeXNEG1D(int x, int y, int z) const{
    return x > 0 &&
            !(*this)(x,y,z).containsSome(CC3DFaceBits::YEDGE | CC3DFaceBits::ZEDGE | CC3DFaceBits::XEDGE) &&
            (*this)(x - 1,y,z).containsSome(CC3DFaceBits::XEDGE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::ZEDGE)) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YEDGE));
}

bool CubicalComplex3D::isFreeXPOS(int x, int y, int z) const{
    return isFreeXPOS1D(x, y, z) || isFreeXPOS2D(x, y, z) || isFreeXPOS3D(x, y, z);
}

bool CubicalComplex3D::isFreeXPOS3D(int x, int y, int z) const{
    return x < int(width()) - 1 &&
            (*this)(x,y,z).containsSome(CC3DFaceBits::CUBE) &&
            (x == 0  || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::CUBE));
}

bool CubicalComplex3D::isFreeXPOS2D(int x, int y, int z) const{
    return isFreeXPOS2DXY(x, y, z) || isFreeXPOS2DXZ(x, y, z);
}

bool CubicalComplex3D::isFreeXPOS2DXY(int x, int y, int z) const{
    return x < int(width()) - 1 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::YZFACE) &&
            (*this)(x, y, z).containsSome(CC3DFaceBits::XYFACE) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XYFACE)) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::YZFACE));
}

bool CubicalComplex3D::isFreeXPOS2DXZ(int x, int y, int z) const{
    return x < int(width()) - 1 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::YZFACE) &&
            (*this)(x, y, z).containsSome(CC3DFaceBits::XZFACE) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XZFACE)) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YZFACE));
}

bool CubicalComplex3D::isFreeXPOS1D(int x, int y, int z) const{
    return x < int(width()) - 1 &&
            !(*this)(x,y,z).containsSome(CC3DFaceBits::YEDGE | CC3DFaceBits::ZEDGE) &&
            (*this)(x,y,z).containsSome(CC3DFaceBits::XEDGE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::ZEDGE)) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YEDGE)) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XEDGE));
}

bool CubicalComplex3D::isFreeYNEG(int x, int y, int z) const{
   return isFreeYNEG1D(x, y, z) || isFreeYNEG2D(x, y, z) || isFreeYNEG3D(x, y, z);
}

bool CubicalComplex3D::isFreeYNEG3D(int x, int y, int z) const{
    return y > 0 &&
            (*this)(x , y - 1, z).containsSome(CC3DFaceBits::CUBE) &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::CUBE);
}

bool CubicalComplex3D::isFreeYNEG2D(int x, int y, int z) const{
    return isFreeYNEG2DXY(x,y,z) || isFreeYNEG2DYZ(x,y,z);
}

bool CubicalComplex3D::isFreeYNEG2DXY(int x, int y, int z) const{
    return y > 0 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XZFACE | CC3DFaceBits::XYFACE) &&
            (*this)(x, y - 1, z).containsSome(CC3DFaceBits::XYFACE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::XZFACE));
}

bool CubicalComplex3D::isFreeYNEG2DYZ(int x, int y, int z) const{
    return y > 0 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XZFACE | CC3DFaceBits::YZFACE) &&
            (*this)(x, y - 1, z).containsSome(CC3DFaceBits::YZFACE) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XZFACE));
}

bool CubicalComplex3D::isFreeYNEG1D(int x, int y, int z) const{
    return y > 0 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XEDGE | CC3DFaceBits::YEDGE | CC3DFaceBits::ZEDGE) &&
            (*this)(x, y - 1, z).containsSome(CC3DFaceBits::YEDGE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::ZEDGE)) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XEDGE));
}

bool CubicalComplex3D::isFreeYPOS(int x, int y, int z) const{
    return isFreeYPOS3D(x,y,z) || isFreeYPOS2D(x,y,z) || isFreeYPOS1D(x,y,z);
}

bool CubicalComplex3D::isFreeYPOS3D(int x, int y, int z) const{
    return y < int(height()) - 1 &&
            (*this)(x,y,z).containsSome(CC3DFaceBits::CUBE) &&
            (y == 0  || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::CUBE));
}

bool CubicalComplex3D::isFreeYPOS2D(int x, int y, int z) const{
    return isFreeYPOS2DXY(x,y,z) || isFreeYPOS2DYZ(x,y,z);
}

bool CubicalComplex3D::isFreeYPOS2DXY(int x, int y, int z) const{
    return y < int(height()) - 1 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XZFACE) &&
            (*this)(x, y, z).containsSome(CC3DFaceBits::XYFACE) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::XYFACE)) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::XZFACE));
}

bool CubicalComplex3D::isFreeYPOS2DYZ(int x, int y, int z) const {
    return y < int(height()) - 1 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XZFACE) &&
            (*this)(x, y, z).containsSome(CC3DFaceBits::YZFACE) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YZFACE)) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XZFACE));
}

bool CubicalComplex3D::isFreeYPOS1D(int x, int y, int z) const{
    return y < int(height()) - 1 &&
            !(*this)(x,y,z).containsSome(CC3DFaceBits::XEDGE | CC3DFaceBits::ZEDGE) &&
            (*this)(x,y,z).containsSome(CC3DFaceBits::YEDGE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::ZEDGE)) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YEDGE)) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XEDGE));
}

bool CubicalComplex3D::isFreeZNEG(int x, int y, int z) const{
    return isFreeZNEG3D(x,y,z) || isFreeZNEG2D(x,y,z) || isFreeZNEG1D(x,y,z);
}

bool CubicalComplex3D::isFreeZNEG3D(int x, int y, int z) const{
     return z > 0 &&
             (*this)(x , y, z - 1).containsSome(CC3DFaceBits::CUBE) &&
             !(*this)(x, y, z).containsSome(CC3DFaceBits::CUBE);
}

bool CubicalComplex3D::isFreeZNEG2D(int x, int y, int z) const{
    return isFreeZNEG2DXZ(x,y,z) || isFreeZNEG2DYZ(x,y,z);
}

bool CubicalComplex3D::isFreeZNEG2DXZ(int x, int y, int z) const{
    return z > 0 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XYFACE | CC3DFaceBits::XZFACE) &&
            (*this)(x, y, z - 1).containsSome(CC3DFaceBits::XZFACE) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::XYFACE));
}

bool CubicalComplex3D::isFreeZNEG2DYZ(int x, int y, int z) const{
    return z > 0 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XYFACE | CC3DFaceBits::YZFACE) &&
            (*this)(x, y, z - 1).containsSome(CC3DFaceBits::YZFACE) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XYFACE));
}

bool CubicalComplex3D::isFreeZNEG1D(int x, int y, int z) const{
    return z > 0 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XEDGE | CC3DFaceBits::YEDGE | CC3DFaceBits::ZEDGE) &&
            (*this)(x, y, z - 1).containsSome(CC3DFaceBits::ZEDGE) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YEDGE)) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XEDGE));
}

bool CubicalComplex3D::isFreeZPOS(int x, int y, int z) const{
    return isFreeZPOS3D(x,y,z) || isFreeZPOS2D(x,y,z) || isFreeZPOS1D(x,y,z);
}

bool CubicalComplex3D::isFreeZPOS3D(int x, int y, int z) const{
    return z < int(depth()) - 1 &&
            (*this)(x,y,z).containsSome(CC3DFaceBits::CUBE) &&
            (z == 0  || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::CUBE));
}

bool CubicalComplex3D::isFreeZPOS2D(int x, int y, int z) const{
    return isFreeZPOS2DXZ(x,y,z) || isFreeZPOS2DYZ(x,y,z);
}

bool CubicalComplex3D::isFreeZPOS2DXZ(int x, int y, int z) const{
    return z < int(depth()) - 1 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XYFACE) &&
            (*this)(x, y, z).containsSome(CC3DFaceBits::XZFACE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::XZFACE)) &&
            (y == 0 || !(*this)(x, y - 1, z ).containsSome(CC3DFaceBits::XYFACE));
}

bool CubicalComplex3D::isFreeZPOS2DYZ(int x, int y, int z) const{
    return z < int(depth()) - 1 &&
            !(*this)(x, y, z).containsSome(CC3DFaceBits::XYFACE) &&
            (*this)(x, y, z).containsSome(CC3DFaceBits::YZFACE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::YZFACE)) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XYFACE));
}

bool CubicalComplex3D::isFreeZPOS1D(int x, int y, int z) const{
    return z < int(depth()) - 1 &&
            !(*this)(x,y,z).containsSome(CC3DFaceBits::XEDGE | CC3DFaceBits::YEDGE) &&
            (*this)(x,y,z).containsSome(CC3DFaceBits::ZEDGE) &&
            (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::ZEDGE)) &&
            (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YEDGE)) &&
            (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XEDGE));
}




using CCPoint = std::tuple<Vec3i, uint32_t, uint32_t>; // Voxel + distance26 + opening26

void computeCurvilinearGraph(const CubicalComplex3D& skeletonCC,
                             const Grid3D<uint32_t> distance26Map,
                             const Grid3D<uint32_t> opening26Map,
                             Grid3D<int>& gridToNode,
                             std::vector<CCPoint>& nodes,
                             Graph& graph) {
    gridToNode = Grid3D<int>(skeletonCC.resolution(), -1);

    skeletonCC.forEach([&](int x, int y, int z, const CubicalComplex3D::value_type& face) {
        if(face.containsSome(CC3DFaceBits::POINT) && !skeletonCC.containsSurfacicPoint(x, y, z)) {
            nodes.push_back(std::make_tuple(Vec3i(x, y, z), distance26Map(x, y, z), opening26Map(x, y, z)));
            gridToNode(x, y, z) = nodes.size() - 1;
            graph.emplace_back();
        }
    });

    skeletonCC.forEach([&](int x, int y, int z, const CubicalComplex3D::value_type& face) {
        auto idx = gridToNode(x, y, z);
        if(idx >= 0) {
            if(face.containsSome(CC3DFaceBits::XEDGE)) {
                auto neighbour = gridToNode(x + 1, y, z);
                if(neighbour >= 0) {
                    graph[neighbour].push_back(idx);
                    graph[idx].push_back(neighbour);
                }
            }
            if(face.containsSome(CC3DFaceBits::YEDGE)) {
                auto neighbour = gridToNode(x, y + 1, z);
                if(neighbour >= 0) {
                    graph[neighbour].push_back(idx);
                    graph[idx].push_back(neighbour);
                }
            }
            if(face.containsSome(CC3DFaceBits::ZEDGE)) {
                auto neighbour = gridToNode(x, y, z + 1);
                if(neighbour >= 0) {
                    graph[neighbour].push_back(idx);
                    graph[idx].push_back(neighbour);
                }
            }
        }
    });
}

Grid3D<uint32_t> computeCurvilinearSkeletonMappingGrid(const CubicalComplex3D& originalObjectCC,
                                                       const Grid3D<uint32_t> opening26Map,
                                                       const std::vector<CCPoint>& nodes) {
    Grid3D<uint32_t> mappingGrid(originalObjectCC.resolution(), UNDEFINED_NODE);

    std::queue<CCPoint> mappingQueue;

    for(auto i = 0u; i < nodes.size(); ++i) {
        mappingGrid(std::get<0>(nodes[i])) = i;
        mappingQueue.push(nodes[i]);
    }

    std::queue<CCPoint> borderVoxels;

    while(!mappingQueue.empty()) {
        auto point = mappingQueue.front();
        mappingQueue.pop();

        foreach6Neighbour(mappingGrid.resolution(), std::get<0>(point), [&](const Vec3i& voxel) {
            bool isBorderVoxel = false;
            if(originalObjectCC(voxel).containsSome(CC3DFaceBits::CUBE) && mappingGrid(voxel) == UNDEFINED_NODE) {
                if(opening26Map(voxel) == std::get<2>(point)) {
                    mappingGrid(voxel) = mappingGrid(std::get<0>(point));
                    mappingQueue.push(std::make_tuple(voxel, std::get<1>(point), std::get<2>(point)));
                } else if(!isBorderVoxel) {
                    isBorderVoxel = true;
                    borderVoxels.push(point);
                }
            }
        });
    }

    while(!borderVoxels.empty()) {
        auto point = borderVoxels.front();
        borderVoxels.pop();

        foreach6Neighbour(mappingGrid.resolution(), std::get<0>(point), [&](const Vec3i& voxel) {
            if(originalObjectCC(voxel).containsSome(CC3DFaceBits::CUBE) && mappingGrid(voxel) == UNDEFINED_NODE) {
                mappingGrid(voxel) = mappingGrid(std::get<0>(point));
                borderVoxels.push(std::make_tuple(voxel, std::get<1>(point), opening26Map(voxel)));
            }
        });
    }

//    while(!mappingQueue.empty()) {
//        auto point = mappingQueue.front();
//        mappingQueue.pop();

//        bool hasOneNeighbourOfHigherOpening = false;

//        foreach26Neighbour(mappingGrid.resolution(), std::get<0>(point), [&](const Vec3i& voxel) {
//            if(originalObjectCC(voxel).containsSome(CC3DFaceBits::CUBE) && mappingGrid(voxel) == UNDEFINED_NODE) {
////                // First version: produce unexpected mapping for some voxel
////                mappingGrid(voxel) = mappingGrid(std::get<0>(point));
////                mappingQueue.push(std::make_tuple(voxel, std::get<1>(point), std::get<2>(point)));


//                // Second version: better, respect the opening map in a sence
//                if(opening26Map(voxel) <= std::get<2>(point)) {
//                    mappingGrid(voxel) = mappingGrid(std::get<0>(point));
//                    mappingQueue.push(std::make_tuple(voxel, std::get<1>(point), opening26Map(voxel)));
//                } else {
//                    hasOneNeighbourOfHigherOpening = true;
//                }
//            }
//        });

//        if(hasOneNeighbourOfHigherOpening) {
//            // If the voxel can't propagate to some voxels (because of the opening condition), relax the condition
//            // and try to propagate it later
//            //if(std::get<2>(point) > 0u) {
//                mappingQueue.push(std::make_tuple(std::get<0>(point), std::get<1>(point), std::get<2>(point) + 1));
//            //}
//        }
//    }

    return mappingGrid;
}

static CurvilinearSkeleton transformToCurvilinearSkeleton(
        const std::vector<CCPoint>& nodes, const Graph& graph,
        const CubicalComplex3D& originalObjectCC, const Grid3D<uint32_t> opening26Map,
        const Mat4f& gridToWorld) {
    CurvilinearSkeleton skeleton;
    auto radiusScale = pow(determinant(Mat3f(gridToWorld)), 1.f / 3);

    for(const auto& point: nodes) {
        auto position = Vec3f(gridToWorld * Vec4f(std::get<0>(point), 1.f));
        auto dist = std::get<1>(point);
        auto radius = 0.f;
        if(dist > 0) {
            radius = radiusScale * dist;
        } else {
            radius = 0.f;
        }
        skeleton.addNode(position, radius);
    }

    skeleton.setGraph(graph);
    skeleton.setGridToWorld(gridToWorld, radiusScale);
    skeleton.setGrid(computeCurvilinearSkeletonMappingGrid(originalObjectCC, opening26Map, nodes));

    return skeleton;
}

CurvilinearSkeleton getCurvilinearSkeleton(const CubicalComplex3D& skeletonCC,
                                           const CubicalComplex3D& originalObjectCC,
                                           const Grid3D<uint32_t> distance26Map,
                                           const Grid3D<uint32_t> opening26Map,
                                           const Mat4f& gridToWorld) {
    std::vector<CCPoint> nodes;
    Graph graph;
    Grid3D<int> gridToNode;

    computeCurvilinearGraph(skeletonCC, distance26Map, opening26Map, gridToNode, nodes, graph);

    return transformToCurvilinearSkeleton(nodes, graph, originalObjectCC, opening26Map, gridToWorld);
}

CurvilinearSkeleton getSegmentedCurvilinearSkeleton(const CubicalComplex3D& skeletonCC,
                                                    const CubicalComplex3D& originalObjectCC,
                                                    const Grid3D<uint32_t> distance26Map,
                                                    const Grid3D<uint32_t> opening26Map,
                                                    const Mat4f& gridToWorld) {

    std::vector<CCPoint> nodes;
    Graph graph;
    Grid3D<int> gridToNode;

    computeCurvilinearGraph(skeletonCC, distance26Map, opening26Map, gridToNode, nodes, graph);

    // Segment the graph based on maximal balls:
    std::vector<uint32_t> nodeIdxArray;
    for(auto i = 0u; i < nodes.size(); ++i) {
        nodeIdxArray.push_back(i);
    }
    std::sort(begin(nodeIdxArray), end(nodeIdxArray), [&](uint32_t lhs, uint32_t rhs) {
//        if(std::get<1>(nodes[lhs]) == std::get<1>(nodes[rhs])) {
//            return std::get<2>(nodes[lhs]) < std::get<2>(nodes[rhs]);
//        }
        return  std::get<1>(nodes[lhs]) > std::get<1>(nodes[rhs]);
    });

    std::vector<int> nodeLabels(nodes.size(), -1);
    std::vector<uint32_t> representatives;
    std::vector<std::vector<uint32_t>> nodeSets;
    uint32_t nextLabel = 0u;

    for(auto nodeIdx: nodeIdxArray) {
        if(nodeLabels[nodeIdx] < 0) {
            auto center = std::get<0>(nodes[nodeIdx]);
            auto radius26 = std::get<1>(nodes[nodeIdx]);

            representatives.push_back(nodeIdx);
            nodeSets.emplace_back();

            std::stack<uint32_t> stack;
            stack.push(nodeIdx);

            while(!stack.empty()) {
                auto nodeIdx = stack.top();
                stack.pop();

                nodeLabels[nodeIdx] = nextLabel;
                nodeSets.back().emplace_back(nodeIdx);

                for(const auto& neighbourIdx: graph[nodeIdx]) {
                    if(nodeLabels[neighbourIdx] < 0) {
                        auto voxel = std::get<0>(nodes[neighbourIdx]);
                        auto v = Vec3u(abs(voxel - center));
                        auto d = max(max(v.x, v.y), v.z);
                        if(d < radius26) {
                            stack.push(neighbourIdx);
                        }
                    }
                }
            }

            ++nextLabel;
        }
    }

    std::vector<CCPoint> newNodes;
    Graph newGraph;

    for(auto labelIdx = 0u; labelIdx < representatives.size(); ++labelIdx) {
        newNodes.emplace_back(nodes[representatives[labelIdx]]);
        newGraph.emplace_back();

        for(auto node: nodeSets[labelIdx]) {
            for(auto neighbourIdx: graph[node]) {
                if(nodeLabels[neighbourIdx] != int(labelIdx)) {
                    auto it = std::find(begin(newGraph.back()), end(newGraph.back()), nodeLabels[neighbourIdx]);
                    if(it == end(newGraph.back())) {
                        newGraph.back().emplace_back(nodeLabels[neighbourIdx]);
                    }
                }
            }
        }
    }

    return transformToCurvilinearSkeleton(newNodes, newGraph, originalObjectCC, opening26Map, gridToWorld);
}

CubicalComplex3D getCubicalComplex(const VoxelGrid& voxelGrid, bool getComplementary) {
    CubicalComplex3D cubicalComplex(voxelGrid.resolution());

    auto w = voxelGrid.resolution().x;
    auto h = voxelGrid.resolution().y;
    auto d = voxelGrid.resolution().z;

    auto processVoxel = [&](const Vec3i& voxel) {
        if (bool(voxelGrid(voxel)) == !getComplementary) {
            auto x = voxel.x;
            auto y = voxel.y;
            auto z = voxel.z;

            cubicalComplex(x, y, z).fill();

            auto value = voxelGrid(x, y, z);

            if (x == w - 1 || value != voxelGrid(x + 1, y, z)) cubicalComplex(x + 1, y, z).add(CC3DFaceBits::POINT | CC3DFaceBits::YEDGE | CC3DFaceBits::ZEDGE | CC3DFaceBits::YZFACE);

            if (y == h - 1 || value != voxelGrid(x, y + 1, z)) cubicalComplex(x, y + 1, z).add(CC3DFaceBits::POINT | CC3DFaceBits::XEDGE | CC3DFaceBits::ZEDGE | CC3DFaceBits::XZFACE);

            if (z == d - 1 || value != voxelGrid(x, y, z + 1)) cubicalComplex(x, y, z + 1).add(CC3DFaceBits::POINT | CC3DFaceBits::XEDGE | CC3DFaceBits::YEDGE | CC3DFaceBits::XYFACE);

            if (x == w - 1 || y == h - 1 || value != voxelGrid(x + 1, y + 1, z)) cubicalComplex(x + 1, y + 1, z).add(CC3DFaceBits::POINT | CC3DFaceBits::ZEDGE);

            if (x == w - 1 || z == d - 1 || value != voxelGrid(x + 1, y, z + 1)) cubicalComplex(x + 1, y, z + 1).add(CC3DFaceBits::POINT | CC3DFaceBits::YEDGE);

            if (y == h - 1 || z == d - 1 || value != voxelGrid(x, y + 1, z + 1)) cubicalComplex(x, y + 1, z + 1).add(CC3DFaceBits::POINT | CC3DFaceBits::XEDGE);

            if (x == w - 1 || y == h - 1 || z == d - 1 || value != voxelGrid(x + 1, y + 1, z + 1)) cubicalComplex(x + 1, y + 1, z + 1).add(CC3DFaceBits::POINT);
        }
    };

    foreachVoxel(voxelGrid.resolution(), processVoxel);

    return cubicalComplex;
}

CubicalComplex3D filterSurfacicParts(const CubicalComplex3D& skeletonCC) {
    CubicalComplex3D filteredSkeleton(skeletonCC.resolution());

    skeletonCC.forEach([&](int x, int y, int z, const CubicalComplex3D::value_type& face) {
        if(skeletonCC.isEdgeXCurvilinear(x, y, z)) {
            filteredSkeleton(x, y, z).add(CC3DFaceBits::XEDGE | CC3DFaceBits::POINT);
            if(x < filteredSkeleton.width() - 1) {
                filteredSkeleton(x + 1, y, z).add(CC3DFaceBits::POINT);
            }
        }
        if(skeletonCC.isEdgeYCurvilinear(x, y, z)) {
            filteredSkeleton(x, y, z).add(CC3DFaceBits::YEDGE | CC3DFaceBits::POINT);
            if(y < filteredSkeleton.height() - 1) {
                filteredSkeleton(x, y + 1, z).add(CC3DFaceBits::POINT);
            }
        }
        if(skeletonCC.isEdgeZCurvilinear(x, y, z)) {
            filteredSkeleton(x, y, z).add(CC3DFaceBits::ZEDGE | CC3DFaceBits::POINT);
            if(z < filteredSkeleton.depth() - 1) {
                filteredSkeleton(x, y, z + 1).add(CC3DFaceBits::POINT);
            }
        }
    });

    return filteredSkeleton;
}

CubicalComplex3D filterIsolatedPoints(const CubicalComplex3D& skeletonCC) {
    CubicalComplex3D filteredSkeleton = skeletonCC;

    filteredSkeleton.forEach([&](int x, int y, int z, const CubicalComplex3D::value_type& face) {
        if(filteredSkeleton.isIsolatedPoint(x, y, z)) {
            filteredSkeleton(x, y, z) = CC3DFace(CC3DFaceBits::NOFACE);
        }
    });

    return filteredSkeleton;
}

}
