#pragma once

#include "Random.hpp"
#include "Sample.hpp"

namespace BnZ {

class DiscreteDistribution4f {
public:
    DiscreteDistribution4f() {
    }

    DiscreteDistribution4f(const Vec4f& weights):
        m_PDF(weights) {
        float sum = weights[0] + weights[1] + weights[2] + weights[3];
        if(sum > 0.f) {
            m_PDF /= sum;
            m_CDF[0] = m_PDF[0];
            m_CDF[1] = m_CDF[0] + m_PDF[1];
            m_CDF[2] = m_CDF[1] + m_PDF[2];
            m_CDF[3] = m_CDF[2] + m_PDF[3];
        } else {
            m_CDF = Vec4f(0.f);
        }
    }

    Sample1u sample(float s) const {
        if(s <= m_CDF[0]) {
            return Sample1u(0, m_PDF[0]);
        }
        if(s <= m_CDF[1]) {
            return Sample1u(1, m_PDF[1]);
        }
        if(s <= m_CDF[2]) {
            return Sample1u(2, m_PDF[2]);
        }
        return Sample1u(3, m_PDF[3]);
    }

    float pdf(uint32_t value) const {
        return m_PDF[value];
    }

private:
    Vec4f m_PDF;
    Vec4f m_CDF;
};

class DiscreteDistribution {
public:
    DiscreteDistribution() {
    }

    DiscreteDistribution(uint32_t count, const float* weights):
        //m_Distribution(weights, weights + count),
        m_Distribution(count, 0, count, [&](double x) -> float {
        uint32_t i = uint32_t(x);
        return weights[i];
    }),
        m_Probabilities(m_Distribution.probabilities()) {
    }

    Sample1u sample(float s) const {
        RandomFloat wrapper(s);
        auto i = m_Distribution(wrapper);
        return Sample1u(i, float(m_Probabilities[i]));
    }

    float pdf(uint32_t value) const {
        return float(m_Probabilities[value]);
    }

private:
    mutable std::discrete_distribution<uint32_t> m_Distribution;
    std::vector<double> m_Probabilities;
};

}
