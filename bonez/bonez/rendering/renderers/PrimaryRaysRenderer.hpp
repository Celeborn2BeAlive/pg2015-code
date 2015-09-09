#pragma once

#include "TileProcessingRenderer.hpp"
#include <bonez/scene/sensors/PixelSensor.hpp>

namespace BnZ {

class PrimaryRaysRenderer: public TileProcessingRenderer {
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

        auto ray = raySample.value;

        accumulate(0, pixelID, Vec4f(ray.dir, 1.f));
    }

    void processTile(uint32_t threadID, uint32_t tileID, const Vec4u& viewport) const override {
        processTileSamples(viewport, [this, threadID](uint32_t x, uint32_t y, uint32_t pixelID, uint32_t sampleID) {
            processSample(threadID, pixelID, sampleID, x, y);
        });
    }

    void initFramebuffer() override {
        addFramebufferChannel("final_render");
    }
};


}
