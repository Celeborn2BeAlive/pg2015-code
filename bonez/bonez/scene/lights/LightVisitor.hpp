#pragma once

namespace BnZ {

class Light;
class PointLight;
class DirectionalLight;
class AreaLight;
class EnvironmentLight;

class LightVisitor {
public:
    virtual ~LightVisitor() {
    }

    virtual void visit(const Light& light) {

    }

    virtual void visit(const PointLight& light) {

    }

    virtual void visit(const DirectionalLight& light) {

    }

    virtual void visit(const AreaLight& light) {

    }

    virtual void visit(const EnvironmentLight& light) {

    }
};

}
