#pragma once

#include <bonez/maths/maths.hpp>

namespace BnZ {

class EmptySpaceVoxelMapping {
public:
    EmptySpaceVoxelMapping();

    // Map a 3D surface point to an empty space voxel, according to an incident direction
    // If no empty space voxel is found in the neighbouring of the point, the voxel containing it is returned.
    // The function bool isInEmptySpace(Vec3i voxel) must be defined
    // "worldToGrid" must be a matrix than applie an homogenous scale (same in each direction) in order to correctly
    // convert the normal to grid space
    template<typename Functor>
    Vec3i getVoxel(const Vec3f& position, Vec3f normal, const Vec3f& incidentDirection,
              const Mat4f& worldToGrid, Functor&& isInEmptySpace) const {
        faceForward(incidentDirection, normal); // Make the normal point in the same hemisphere than indicentDirection

        auto pointInGrid = worldToGrid * Vec4f(position, 1.f);

        // We suppose here that the world to grid matrix applies
        // the same scale in each direction:
        auto N = normalize(Vec3f(worldToGrid * Vec4f(normal, 0.f)));
        auto wi = normalize(Vec3f(worldToGrid * Vec4f(incidentDirection, 0.f)));

        CubeFace face = getCubeFace(N);

        auto voxel = Vec3i(pointInGrid);
        auto bestNeighbour = voxel;
        Vec3i bestOffset;
        auto bestDotWithNormal = 0.f;

        auto bestDotWithIncidentDirection = 0.f;

        for(const auto& offset: m_FaceOffsets[int(face)]) {
            auto neighbour = voxel + offset;
            if(isInEmptySpace(neighbour)) {
                auto dir = normalize(Vec3f(neighbour - voxel));
                auto d = dot(N, dir);
                if(d > bestDotWithNormal) {
                    bestDotWithNormal = d;
                    bestOffset = offset;
                    bestNeighbour = neighbour;
                    bestDotWithIncidentDirection = dot(wi, dir);
                } else if(d == bestDotWithNormal) {
                    // ambuiguity, use the incident direction to decide
                    auto d2 = dot(wi, dir);
                    if(d2 > bestDotWithIncidentDirection) {
                        bestDotWithNormal = d;
                        bestNeighbour = neighbour;
                        bestDotWithIncidentDirection = d2;
                    }
                }
            }
        }

        return bestNeighbour;
    }

    // Same function, to call if not incident direction is avalaible
    template<typename Functor>
    Vec3i getVoxel(const Vec3f& position, Vec3f normal,
              const Mat4f& worldToGrid, Functor&& isInEmptySpace) const {
        auto pointInGrid = worldToGrid * Vec4f(position, 1.f);

        // We suppose here that the world to grid matrix applies
        // the same scale in each direction:
        auto N = normalize(Vec3f(worldToGrid * Vec4f(normal, 0.f)));

        CubeFace face = getCubeFace(N);

        auto voxel = Vec3i(pointInGrid);
        auto bestNeighbour = voxel;
        auto bestDot = 0.f;

        for(const auto& offset: m_FaceOffsets[int(face)]) {
            auto neighbour = voxel + offset;
            if(isInEmptySpace(neighbour)) {
                auto dir = normalize(Vec3f(neighbour - voxel));
                auto d = dot(N, dir);
                if(d > bestDot) {
                    bestDot = d;
                    bestNeighbour = neighbour;
                }
            }
        }

        return bestNeighbour;
    }

    // Just returns the voxel containing the position
    Vec3i getVoxel(const Vec3f& position, const Mat4f& worldToGrid) const {
        return Vec3i(worldToGrid * Vec4f(position, 1));
    }

private:
    Vec3i m_FaceOffsets[int(CubeFace::FACE_COUNT)][9]; // Offsets of voxel neighbours for each face
};

}
