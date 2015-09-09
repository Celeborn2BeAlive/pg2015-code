#pragma once

#include <vector>
#include <bonez/maths/types.hpp>

namespace BnZ {

struct PointLightVPL {
    Vec3f m_Position;
    Vec3f m_Intensity;
};

struct DirectionalLightVPL {
    Vec3f m_IncidentDirection;
    Vec3f m_Intensity;
};

struct AreaLightVPL {
    Vec3f m_Position;
    Vec3f m_Normal;
    Vec3f m_Intensity;
};

class EmissionVPLContainer {
public:
    using PointLightVPLArray = std::vector<PointLightVPL>;
    using DirectionalLightVPLArray = std::vector<DirectionalLightVPL>;
    using AreaLightVPLArray = std::vector<AreaLightVPL>;

    void add(const PointLightVPL& vpl) {
        m_PointLightVPLArray.emplace_back(vpl);
    }

    void add(const DirectionalLightVPL& vpl) {
        m_DirectionalLightVPLArray.emplace_back(vpl);
    }

    void add(const AreaLightVPL& vpl) {
        m_AreaLightVPLArray.emplace_back(vpl);
    }

    const PointLightVPLArray& getPointLightVPLArray() const {
        return m_PointLightVPLArray;
    }

    const DirectionalLightVPLArray& getDirectionalLightVPLArray() const {
        return m_DirectionalLightVPLArray;
    }

    const AreaLightVPLArray& getAreaLightVPLArray() const {
        return m_AreaLightVPLArray;
    }

    size_t size() const {
        return m_PointLightVPLArray.size() + m_DirectionalLightVPLArray.size() + m_AreaLightVPLArray.size();
    }

    bool empty() const {
        return m_PointLightVPLArray.empty() && m_DirectionalLightVPLArray.empty() && m_AreaLightVPLArray.empty();
    }

    void clear() {
        m_PointLightVPLArray.clear();
        m_DirectionalLightVPLArray.clear();
        m_AreaLightVPLArray.clear();
    }

private:
    PointLightVPLArray m_PointLightVPLArray;
    DirectionalLightVPLArray m_DirectionalLightVPLArray;
    AreaLightVPLArray m_AreaLightVPLArray;
};

}
