#pragma once

#include "Sensor.hpp"
#include <bonez/scene/Scene.hpp>

namespace BnZ {

class PixelSensor: public Sensor {
public:
    PixelSensor(const Sensor& sensor, const Vec2u& pixel, const Vec2u& framebufferSize):
        m_Sensor(sensor),
        m_fRcpJacobianRasterToImage(framebufferSize.x * framebufferSize.y),
        m_fPixel(pixel),
        m_fFramebufferSize(framebufferSize) {
    }

    Vec3f sampleExitantRay(
                    const Scene& scene,
                    const Vec2f& lensSample,
                    const Vec2f& pixelSample,
                    RaySample& raySample,
                    float& positionPdf,
                    float& directionPdf,
                    Intersection& I,
                    float& sampledPointToIncidentDirectionJacobian,
                    float& intersectionPdfWrtArea) const override {
        auto rasterSample = m_fPixel + pixelSample;
        auto imageSample = rasterSample / m_fFramebufferSize;
        auto pdfImageSample = m_fRcpJacobianRasterToImage;

        auto We = m_Sensor.sampleExitantRay(scene, lensSample, imageSample, raySample, positionPdf, directionPdf,
                                            I, sampledPointToIncidentDirectionJacobian, intersectionPdfWrtArea);

        raySample.pdf *= pdfImageSample;
        directionPdf *= pdfImageSample;
        intersectionPdfWrtArea *= pdfImageSample;

        // The box-filter function is constant and equal to the pdf such that it cancels
        // during Monte-Carlo estimation
        return We * m_fRcpJacobianRasterToImage;
    }

    // Sample an exitant ray
    Vec3f sampleExitantRay(
                const Scene& scene,
                const Vec2f& lensSample,
                const Vec2f& pixelSample,
                RaySample& raySample,
                float& positionPdf,
                float& directionPdf) const override {
        auto rasterSample = m_fPixel + pixelSample;
        auto imageSample = rasterSample / m_fFramebufferSize;
        auto pdfImageSample = m_fRcpJacobianRasterToImage;

        auto We = m_Sensor.sampleExitantRay(scene, lensSample, imageSample, raySample, positionPdf, directionPdf);

        raySample.pdf *= pdfImageSample;
        directionPdf *= pdfImageSample;

        // The box-filter function is constant and equal to the pdf such that it cancels
        // during Monte-Carlo estimation
        return We * m_fRcpJacobianRasterToImage;
    }

    // Sample the direct importance on point
    // Compute a shadow ray sample on the hemisphere of point with pdf wrt solid angle
    // Compute the imageSample associated to lensSample that gived the exitant ray reaching point
    Vec3f sampleDirectImportance(const Scene& scene,
                                 const Vec2f& lensSample,
                                 const SurfacePoint& point,
                                 RaySample& shadowRay, // Return the shadowRay on point's hemisphere, with pdf wrt. solid angle
                                 float* sampledPointToIncidentDirectionJacobian = nullptr,
                                 float* pdfWrtArea = nullptr, // Return the pdf of sampling this point, wrt area
                                 float* revPdfWrtSolidAngle = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt solid angle
                                 float* revPdfWrtArea = nullptr, // Return the pdf of sampling "point" from the sampled point, wrt area
                                 Vec2f* imageSample = nullptr) const override {
        Vec2f cameraImageSample;
        auto We = m_Sensor.sampleDirectImportance(scene,
                                                  lensSample,
                                                  point,
                                                  shadowRay,
                                                  sampledPointToIncidentDirectionJacobian,
                                                  pdfWrtArea,
                                                  revPdfWrtSolidAngle,
                                                  revPdfWrtArea,
                                                  &cameraImageSample);
        auto pixelSample = cameraImageSample * m_fFramebufferSize - m_fPixel;
        if(pixelSample.x < 0.f || pixelSample.x > 1.f || pixelSample.y < 0.f || pixelSample.y > 1.f) {
            return zero<Vec3f>();
        }

        auto pdfImageSample = m_fRcpJacobianRasterToImage;

        if(imageSample) {
            *imageSample = pixelSample;
        }
        if(revPdfWrtSolidAngle) {
            *revPdfWrtSolidAngle *= pdfImageSample;
        }
        if(revPdfWrtArea) {
            *revPdfWrtArea *= pdfImageSample;
        }

        return We * m_fRcpJacobianRasterToImage;
    }

private:
    const Sensor& m_Sensor;
    float m_fRcpJacobianRasterToImage;
    Vec2f m_fPixel;
    Vec2f m_fFramebufferSize;
};

}
