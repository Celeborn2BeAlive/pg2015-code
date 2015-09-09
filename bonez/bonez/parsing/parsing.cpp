#include "parsing.hpp"
#include <bonez/scene/Scene.hpp>

namespace BnZ {

Vec3f loadVector(const tinyxml2::XMLElement& elt) {
    Vec3f v;
    getAttribute(elt, "x", v.x);
    getAttribute(elt, "y", v.y);
    getAttribute(elt, "z", v.z);
    return v;
}

Vec3f loadColor(const tinyxml2::XMLElement& elt, const Vec3f& defaultColor) {
    Vec3f c;
    if(!getAttribute(elt, "r", c.r)) {
        c.r = defaultColor.r;
    }
    if(!getAttribute(elt, "g", c.g)) {
        c.g = defaultColor.g;
    }
    if(!getAttribute(elt, "b", c.b)) {
        c.b = defaultColor.b;
    }
    return c;
}

void storeVector(tinyxml2::XMLElement& elt, const Vec3f& v) {
    setAttribute(elt, "x", v.x);
    setAttribute(elt, "y", v.y);
    setAttribute(elt, "z", v.z);
}

void storeColor(tinyxml2::XMLElement& elt, const Vec3f& c) {
    setAttribute(elt, "r", c.r);
    setAttribute(elt, "g", c.g);
    setAttribute(elt, "b", c.b);
}

PointLight loadPointLight(
        const tinyxml2::XMLElement& elt) {
    return { loadVector(elt), loadColor(elt) };
}

DirectionalLight loadDirectionalLight(
        const Scene& scene,
        const tinyxml2::XMLElement& elt) {
    auto pExitantPower = elt.FirstChildElement("ExitantPower");
    if(pExitantPower) {
        Vec3f wi(0, 1, 0);
        getChildAttribute(elt, "IncidentDirection", wi);
        Vec3f exitantPower = zero<Vec3f>();
        getChildAttribute(elt, "ExitantPower", exitantPower);

        return DirectionalLight(wi, exitantPower, scene);
    }

    return { normalize(loadVector(elt)), loadColor(elt) };
}

EnvironmentLight loadEnvironmentLight(
        const Scene& scene,
        const tinyxml2::XMLElement& elt) {
    auto pBuildFromDirection = elt.FirstChildElement("BuildFromDirection");
    if(pBuildFromDirection) {
        Vec3f wi(0, 1, 0);
        getChildAttribute(*pBuildFromDirection, "IncidentDirection", wi);
        Vec3f exitantPower = zero<Vec3f>();
        getChildAttribute(*pBuildFromDirection, "ExitantPower", exitantPower);
        Vec2u resolution = Vec2u(1, 1);
        getChildAttribute(*pBuildFromDirection, "Resolution", resolution);

        return EnvironmentLight(resolution, wi, exitantPower, scene);
    }

    throw std::runtime_error("Unable to load EnvironmentLight");
}

void loadLights(Scene& scene, const tinyxml2::XMLElement* pLightElt) {
    if(pLightElt) {
        for(auto pLight = pLightElt->FirstChildElement();
            pLight;
            pLight = pLight->NextSiblingElement()) {
            auto lightType = toString(pLight->Name());
            if(lightType == "PointLight") {
                scene.addLight(makeShared<PointLight>(loadPointLight(*pLight)));
            } else if(lightType == "DirectionalLight") {
                scene.addLight(makeShared<DirectionalLight>(loadDirectionalLight(scene, *pLight)));
            } else if(lightType == "EnvironmentLight") {
                scene.addLight(makeShared<EnvironmentLight>(loadEnvironmentLight(scene, *pLight)));
            } else {
                std::cerr << "Unrecognized light type" << lightType << std::endl;
            }
        }
    }
}

class LightStoreVisitor: public LightVisitor {
    tinyxml2::XMLElement& m_LightElmt;
public:
    LightStoreVisitor(tinyxml2::XMLElement& lightElmt): m_LightElmt(lightElmt) {
    }

    virtual void visit(const Light& light) {
        std::cerr << "LightStoreVisitor: unknown light type" << std::endl;
    }

    template<typename LightType>
    void doVisit(const LightType& light) {
        auto elt = m_LightElmt.GetDocument()->NewElement("Unknown");
        m_LightElmt.InsertEndChild(elt);
        storeLight(*elt, light);
    }

    virtual void visit(const PointLight& light) override {
        doVisit(light);
    }

    virtual void visit(const DirectionalLight& light) {
        doVisit(light);
    }
};

void storeLights(const LightContainer& lightContainer, tinyxml2::XMLElement& lightElt) {
    LightStoreVisitor lightStore(lightElt);
    lightContainer.accept(lightStore);
}

void storeLight(
        tinyxml2::XMLElement& elt,
        const PointLight& light) {
    storeVector(elt, light.m_Position);
    storeColor(elt, light.m_Intensity);
    elt.SetName("PointLight");
}

void storeLight(
        tinyxml2::XMLElement& elt,
        const DirectionalLight& light) {
    storeVector(elt, light.m_IncidentDirection);
    storeColor(elt, light.m_Le);
    elt.SetName("DirectionalLight");
}

void loadPerspectiveCamera(
        const tinyxml2::XMLElement& elt,
        Vec3f& eye, Vec3f& point, Vec3f& up,
        float& fovY) {
    getAttribute(elt, "fovY", fovY);
    fovY = radians(fovY);
    getAttribute(elt, "eyeX", eye.x);
    getAttribute(elt, "eyeY", eye.y);
    getAttribute(elt, "eyeZ", eye.z);
    getAttribute(elt, "pointX", point.x);
    getAttribute(elt, "pointY", point.y);
    getAttribute(elt, "pointZ", point.z);
    getAttribute(elt, "upX", up.x);
    getAttribute(elt, "upY", up.y);
    getAttribute(elt, "upZ", up.z);
}

void storePerspectiveCamera(
        tinyxml2::XMLElement& elt,
        const Vec3f& eye, const Vec3f& point, const Vec3f& up,
        float fovY) {
    setAttribute(elt, "fovY", degrees(fovY));
    setAttribute(elt, "eyeX", eye.x);
    setAttribute(elt, "eyeY", eye.y);
    setAttribute(elt, "eyeZ", eye.z);
    setAttribute(elt, "pointX", point.x);
    setAttribute(elt, "pointY", point.y);
    setAttribute(elt, "pointZ", point.z);
    setAttribute(elt, "upX", up.x);
    setAttribute(elt, "upY", up.y);
    setAttribute(elt, "upZ", up.z);
}

void storePerspectiveCamera(
        tinyxml2::XMLElement& elt,
        const ProjectiveCamera& camera) {
    auto eye = camera.getOrigin();
    storePerspectiveCamera(
                elt,
                eye,
                eye + camera.getFrontVector(),
                camera.getUpVector(),
                camera.getFovY());
}

}
