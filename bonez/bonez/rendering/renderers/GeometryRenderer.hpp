#pragma once

#include "TileProcessingRenderer.hpp"
#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

class GeometryRenderer: public TileProcessingRenderer {
public:
    void processSample(uint32_t threadID, uint32_t pixelID, uint32_t sampleID,
                       uint32_t x, uint32_t y) const {
        PixelSensor sensor(getSensor(), Vec2u(x, y), getFramebufferSize());

        auto pixelSample = getPixelSample(threadID, sampleID);
        auto lensSample = getFloat2(threadID);

        RaySample raySample;
        Intersection I;

        sampleExitantRay(
                    sensor,
                    getScene(),
                    lensSample,
                    pixelSample,
                    raySample,
                    I);

        if(I) {
            auto bbox = getScene().getBBox();
            auto l = size(bbox);

            accumulate(0, pixelID, Vec4f((I.P - bbox.lower) / l, 1.f));
            accumulate(1, pixelID, Vec4f(I.Ns, 1.f));
            accumulate(2, pixelID, Vec4f(abs(I.Ns), 1.f));
            accumulate(3, pixelID, Vec4f(I.Ng, 1.f));
            accumulate(4, pixelID, Vec4f(abs(I.Ng), 1.f));
            accumulate(5, pixelID, Vec4f(Vec3f(I.distance / length(l)), 1.f));
            accumulate(6, pixelID, Vec4f(I.uv, 0.f, 1.f));
            accumulate(7, pixelID, Vec4f(I.texCoords, 0.f, 1.f));
            accumulate(8, pixelID, Vec4f(I.Le, 1.f));
        }
    }

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override {
        processTileSamples(viewport, [this, threadID](uint32_t x, uint32_t y, uint32_t pixelID, uint32_t sampleID) {
            processSample(threadID, pixelID, sampleID, x, y);
        });
    }

    void initFramebuffer() override {
        addFramebufferChannel("normalized_position");
        addFramebufferChannel("shading_normal");
        addFramebufferChannel("abs_shading_normal");
        addFramebufferChannel("geometric_normal");
        addFramebufferChannel("abs_geometric_normal");
        addFramebufferChannel("normalized_distance");
        addFramebufferChannel("uv");
        addFramebufferChannel("texture_coords");
        addFramebufferChannel("emitted_radiance");
    }
};


}
