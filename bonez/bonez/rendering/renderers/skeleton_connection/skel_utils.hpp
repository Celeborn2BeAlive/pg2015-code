#pragma once

#include <bonez/types.hpp>
#include <bonez/scene/topology/CurvilinearSkeleton.hpp>

namespace BnZ {

inline float computeNodeWeightDistCosMaxball(float dist, Vec3f wi, Vec3f N_ws, float r) {
    float c = max(0.0001f, dot(wi, N_ws)); // Works because node view matrix are only translations
    return r * c / (dist * dist);
}

template<typename SkelNodeOcclusionFunction>
Vec4i getNearestNodes(const CurvilinearSkeleton& skel, const Vec3f& P, const Vec3f& N, Vec4f& weights, const SkelNodeOcclusionFunction& occluded) {
    Vec4i nearestNodes = Vec4i(-1);
    Vec4f currentWeights = Vec4f(0);

    for(auto i = 0u; i < skel.size(); ++i) {
        auto nodeP = skel.getNode(i).P;

        auto dir = P - nodeP;
        auto dist = length(dir);
        dir /= dist;

        auto wi = -dir;
        float weight = computeNodeWeightDistCosMaxball(dist, wi, N, skel.getNode(i).maxball);
        if(weight > currentWeights.x || weight > currentWeights.y || weight > currentWeights.z || currentWeights.w) {
            bool occ= occluded(i, dir, dist);

            if(!occ) {
                if(weight > currentWeights.x) {
                    for(int i = 3; i > 0; --i) {
                        currentWeights[i] = currentWeights[i - 1];
                        nearestNodes[i] = nearestNodes[i - 1];
                    }
                    currentWeights.x = weight;
                    nearestNodes.x = i;
                } else if(weight > currentWeights.y) {
                    for(int i = 3; i > 1; --i) {
                        currentWeights[i] = currentWeights[i - 1];
                        nearestNodes[i] = nearestNodes[i - 1];
                    }
                    currentWeights.y = weight;
                    nearestNodes.y = i;
                } else if(weight > currentWeights.z) {
                    for(int i = 3; i > 2; --i) {
                        currentWeights[i] = currentWeights[i - 1];
                        nearestNodes[i] = nearestNodes[i - 1];
                    }
                    currentWeights.z = weight;
                    nearestNodes.z = i;
                } else if(weight > currentWeights.w) {
                    currentWeights.w = weight;
                    nearestNodes.w = i;
                }
            }
        }
    }

    weights = currentWeights;
    return nearestNodes;
}

template<typename SkelNodeOcclusionFunction>
int getNearestNode(const CurvilinearSkeleton& skel, const Vec3f& P, const Vec3f& N, const SkelNodeOcclusionFunction& occluded) {
    int nearestNode = -1;
    float currentWeight = 0;

    for(auto i = 0u; i < skel.size(); ++i) {
        auto nodeP = skel.getNode(i).P;

        auto dir = P - nodeP;
        auto dist = length(dir);
        dir /= dist;

        auto wi = -dir;
        float weight = computeNodeWeightDistCosMaxball(dist, wi, N, skel.getNode(i).maxball);
        if(weight > currentWeight) {
            bool occ = occluded(i, dir, dist);

            if(!occ) {
                if(weight > currentWeight) {
                    currentWeight = weight;
                    nearestNode = i;
                }
            }
        }
    }
    return nearestNode;
}

inline Sample1u sampleNodeFromWeights(
        const Vec4i& nodeIndices,
        const Vec4f& nodeWeights,
        uint32_t maxNodeCount,
        float s1D) {
    auto size = 0u;
    Vec4f weights = zero<Vec4f>();
    for (size = 0u;
         size < min(maxNodeCount, 4u)
         && nodeIndices[size] >= 0;
         ++size) {
        weights[size] = nodeWeights[size];
    }

    if(size > 0) {
        DiscreteDistribution4f dist(weights);
        return dist.sample(s1D);
    }

    return Sample1u();
}

}
