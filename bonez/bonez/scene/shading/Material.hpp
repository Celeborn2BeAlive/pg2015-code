#pragma once

#include <bonez/types.hpp>
#include "bonez/image/Image.hpp"

namespace BnZ {

struct Material {
    std::string m_sName;

    Vec3f m_DiffuseReflectance = Vec3f(1.f);
    Vec3f m_GlossyReflectance = Vec3f(0.f);
    Vec3f m_EmittedRadiance = Vec3f(0.f);
    float m_Shininess = 1.f;
    Vec3f m_SpecularReflectance = Vec3f(0.f);
    Vec3f m_SpecularTransmittance = Vec3f(0.f);
    float m_fIndexOfRefraction = -1.f; // Index of refraction | -1 = perfect specular reflexion, else: mix between reflexion and refraction
    float m_fSpecularAbsorption = 0.f;

    Shared<Image> m_DiffuseReflectanceTexture;
    Shared<Image> m_GlossyReflectanceTexture;
    Shared<Image> m_ShininessTexture;
    Shared<Image> m_EmittedRadianceTexture;
    Shared<Image> m_SpecularReflectanceTexture;
    Shared<Image> m_SpecularTransmittanceTexture;
    Shared<Image> m_IndexOfRefractionTexture;
    Shared<Image> m_SpecularAbsorptionTexture;

    Material(const std::string& name): m_sName(name) {
    }

    const std::string& getName() const {
        return m_sName;
    }

    static Vec3f getComponent(const Vec2f& texCoords, Vec3f constant, const Shared<Image>& image) {
        if(image) {
            constant *= Vec3f(texture(*image, texCoords));
        }
        return constant;
    }

    static float getComponent(const Vec2f& texCoords, float constant, const Shared<Image>& image) {
        if(image) {
            constant *= texture(*image, texCoords).r;
        }
        return constant;
    }

    Vec3f getDiffuseReflectance(const Vec2f& texCoords) const {
        return getComponent(texCoords, m_DiffuseReflectance, m_DiffuseReflectanceTexture);
    }

    Vec3f getGlossyReflectance(const Vec2f& texCoords) const {
        return getComponent(texCoords, m_GlossyReflectance, m_GlossyReflectanceTexture);
    }

    Vec3f getSpecularReflectance(const Vec2f& texCoords) const {
        return getComponent(texCoords, m_SpecularReflectance, m_SpecularReflectanceTexture);
    }

    Vec3f getSpecularTransmittance(const Vec2f& texCoords) const {
        return getComponent(texCoords, m_SpecularTransmittance, m_SpecularTransmittanceTexture);
    }

    Vec3f getEmittedRadiance(const Vec2f& texCoords) const {
        return getComponent(texCoords, m_EmittedRadiance, m_EmittedRadianceTexture);
    }

    float getShininess(const Vec2f& texCoords) const {
        return getComponent(texCoords, m_Shininess, m_ShininessTexture);
    }

    float getIndexOfRefraction(const Vec2f& texCoords) const {
        return getComponent(texCoords, m_fIndexOfRefraction, m_IndexOfRefractionTexture);
    }

    float getSpecularAbsorption(const Vec2f& texCoords) const {
        return getComponent(texCoords, m_fSpecularAbsorption, m_SpecularAbsorptionTexture);
    }
};

inline bool isEmissive(const Material& material) {
    return material.m_EmittedRadiance != Vec3f(0.f) || material.m_EmittedRadianceTexture;
}

}
