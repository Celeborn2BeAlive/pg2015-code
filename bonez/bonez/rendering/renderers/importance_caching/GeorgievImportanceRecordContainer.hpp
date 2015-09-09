#pragma once

#include <vector>
#include <bonez/utils/KdTree.hpp>
#include "GeorgievImportanceRecord.hpp"

namespace BnZ {

class GeorgievImportanceRecordContainer: std::vector<ImportanceRecord> {
    using ParentClass = std::vector<ImportanceRecord>;
public:
    using ParentClass::iterator;
    using ParentClass::const_iterator;
    using ParentClass::size;
    using ParentClass::empty;
    using ParentClass::operator [];
    using ParentClass::begin;
    using ParentClass::end;
    using ParentClass::reserve;
    using ParentClass::emplace_back;
    using ParentClass::push_back;

    void clear() {
        ParentClass::clear();
        m_ImportanceRecordsKdTree.clear();
    }

    void buildImportanceRecordsKdTree();

    const KdTree& getImportanceRecordsKdTree() const {
        return m_ImportanceRecordsKdTree;
    }

    struct NearestUnfilteredImportanceRecord {
        std::uint32_t m_nIndex;
        float m_fMetric;

        NearestUnfilteredImportanceRecord() = default;

        NearestUnfilteredImportanceRecord(uint32_t irIndex, float metric): m_nIndex(irIndex), m_fMetric(metric) {
        }

        inline friend bool operator <(const NearestUnfilteredImportanceRecord& lhs, const NearestUnfilteredImportanceRecord& rhs) {
            return lhs.m_fMetric < rhs.m_fMetric;
        }
    };

    std::size_t getNearestImportanceRecords(
            const SurfacePoint& point,
            std::size_t unfilteredIRCount,
            NearestUnfilteredImportanceRecord* pUnfilteredIRBuffer,
            std::size_t filteredIRCount,
            std::uint32_t* pIRIndexBuffer,
            float orientationTradeOff) const;
private:
    KdTree m_ImportanceRecordsKdTree;
};

}
