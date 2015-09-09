#pragma once

#include <vector>
#include <bonez/utils/Grid3D.hpp>
#include <stack>

namespace BnZ {

    template<typename VoxelValueType>
    class Grid3DMinTree;

    template<typename VoxelValueType>
    class Grid3DMinTreeRegion {
	public:
        using value_type = VoxelValueType;

		size_t getParentNode() const {
			return m_nParentNode;
		}

		size_t getChildrenCount() const {
			return m_nChildrenCount;
		}

		size_t getChildrenOffset() const {
			return m_nChildrenOffset;
		}
	private:
        friend class Grid3DMinTree<VoxelValueType>;

        Grid3DMinTreeRegion(size_t label, size_t parentNode,
                            size_t childrenCount, size_t childrenOffset,
                            size_t voxelCount, size_t voxelOffset,
                            VoxelValueType value):
            m_nLabel(label), m_nParentNode(parentNode),
            m_nChildrenCount(childrenCount), m_nChildrenOffset(childrenOffset),
            m_nVoxelCount(voxelCount), m_nVoxelOffset(voxelOffset),
            m_nValue(value) {
        }

        size_t m_nLabel;
		size_t m_nParentNode;
		size_t m_nChildrenCount;
		size_t m_nChildrenOffset;
        size_t m_nVoxelCount;
        size_t m_nVoxelOffset;
        VoxelValueType m_nValue;
	};

    template<typename VoxelValueType>
    class Grid3DMinTree: std::vector<Grid3DMinTreeRegion<VoxelValueType>> {
        using Base = std::vector<Grid3DMinTreeRegion<VoxelValueType>>;
	public:
        using value_type = typename Base::value_type;
        using reference = typename Base::reference;
        using const_reference = typename Base::const_reference;
        using difference_type = typename Base::difference_type;
        using size_type = typename Base::size_type;
        using iterator = typename Base::iterator;
        using const_iterator = typename Base::const_iterator;
		using Base::data;
		using Base::size;
		using Base::empty;
		using Base::begin;
		using Base::end;
		using Base::operator [];

        Grid3DMinTree(const Grid3D<VoxelValueType>& grid) {
			build(grid);
		}
    private:
        void build(const Grid3D<VoxelValueType>& grid) {
            // Clear current state
            Base::clear();
            m_ChildrenArray.clear();
            m_VoxelArray.clear();
            const auto noLabel = std::numeric_limits<size_t>::max();
            m_RegionLabeling = Grid3D<size_t>(grid.resolution(), noLabel);

            std::stack<size_t> voxelStack;
            std::vector<VoxelValueType> allValues;

            // Perform a depth first search from each voxel to compute the connected component
            // based on opening
            foreachVoxel(m_RegionLabeling.resolution(), [&](const Vec3i& voxel) {
                if (m_RegionLabeling(voxel) == noLabel) {
                    auto label = size();
                    auto voxelOffset = m_VoxelArray.size();
                    auto value = grid(voxel);

                    m_RegionLabeling(voxel) = label;
                    voxelStack.push(m_RegionLabeling.offset(voxel));
                    m_VoxelArray.emplace_back(m_RegionLabeling.offset(voxel));

                    while(!voxelStack.empty()) {
                        auto voxel = m_RegionLabeling.coords(voxelStack.top());
                        voxelStack.pop();

                        foreach6Neighbour(m_RegionLabeling.resolution(), voxel, [&](const Vec3i& neighbour) {
                            if(m_RegionLabeling(neighbour) == noLabel
                                    && grid(neighbour) == value) {
                                m_RegionLabeling(neighbour) = label;
                                voxelStack.push(m_RegionLabeling.offset(neighbour));
                                m_VoxelArray.emplace_back(m_RegionLabeling.offset(neighbour));
                            }
                        });
                    }

                    Base::emplace_back(label, std::numeric_limits<size_t>::max(),
                                       std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max(),
                                       m_VoxelArray.size() - voxelOffset, voxelOffset,
                                       value);
                }
            });
        }

		std::vector<size_t> m_ChildrenArray;
        std::vector<size_t> m_VoxelArray;
        Grid3D<size_t> m_RegionLabeling;
	};

}
