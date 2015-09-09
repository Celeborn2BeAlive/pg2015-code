#pragma once

#include <bonez/sampling/DiscreteDistribution.hpp>
#include <bonez/common.hpp>
#include <bonez/sys/memory.hpp>
#include <bonez/viewer/GUI.hpp>

#include "Light.hpp"
#include "LightVisitor.hpp"

namespace BnZ {

class LightContainer {
public:
private:
    void setUpdate() {
        m_UpdateFlag.addUpdate();
    }

    using LightVector = std::vector<Shared<Light>>;
public:
    using const_iterator = LightVector::const_iterator;

    bool hasChanged(UpdateFlag& flag) const {
        if(flag != m_UpdateFlag) {
            flag = m_UpdateFlag;
            return true;
        }
        return false;
    }

    void clear() {
        m_Lights.clear();
        setUpdate();
    }

    bool empty() const {
        return m_Lights.empty();
    }

    uint32_t addLight(const Shared<Light>& pLight) {
        m_Lights.emplace_back(pLight);
        setUpdate();
        return uint32_t(m_Lights.size()) - 1;
    }

    void removeLight(uint32_t idx) {
        m_Lights.erase(std::begin(m_Lights) + idx);
        setUpdate();
    }

    size_t size() const {
        return m_Lights.size();
    }

    const Shared<Light>& getLight(uint32_t idx) const {
        assert(idx < m_Lights.size());
        return m_Lights[idx];
    }

    int getLightID(const Light* pLight) const {
        auto it = std::find_if(std::begin(m_Lights), std::end(m_Lights), [pLight](const Shared<Light>& pCandidate) {
            return pCandidate.get() == pLight;
        });
        if(it == std::end(m_Lights)) {
            return -1;
        }
        return int(it - std::begin(m_Lights));
    }

    void exposeIO(GUI& gui) {
        gui.addValue("light count", uint32_t(size()));
        gui.addVarRW(BNZ_GUI_VAR(m_nSelectedLight));
        m_nSelectedLight = min(m_nSelectedLight, uint32_t(size()));
        gui.addButton("remove", [&]() {
            removeLight(m_nSelectedLight);
        });
        gui.addSeparator();
        if(m_nSelectedLight < size()) {
            const auto& pLight = getLight(m_nSelectedLight);
            pLight->exposeIO(gui, m_UpdateFlag);
        }
    }

    void accept(LightVisitor& visitor) const {
        for(const auto& pLight: m_Lights) {
            pLight->accept(visitor);
        }
    }

    const_iterator begin() const {
        return std::begin(m_Lights);
    }

    const_iterator end() const {
        return std::end(m_Lights);
    }

    template<typename LightType, typename Function>
    class LightFilterVisitor: public LightVisitor {
    public:
        Function* m_pFunction = nullptr;
        std::size_t m_nIndex = 0u;

        virtual void visit(const LightType& light) {
            (*m_pFunction)(m_nIndex, light);
        }
    };

    template<typename LightType, typename Function>
    void forEach(Function&& f) const {
        LightFilterVisitor<LightType, Function> visitor;
        visitor.m_pFunction = &f;
        for(auto i: range(std::size(m_Lights))) {
            visitor.m_nIndex = i;
            m_Lights[i]->accept(visitor);
        }
    }

private:
    UpdateFlag m_UpdateFlag;

    uint32_t m_nSelectedLight = 0;

    LightVector m_Lights;
};

}
