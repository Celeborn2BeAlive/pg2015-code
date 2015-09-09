#include "Image.hpp"

#include <iostream>
#include <fstream>
#include <unordered_map>

#include <bonez/utils/itertools/itertools.hpp>
#include <bonez/maths/maths.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


#include <bonez/sys/DebugLog.hpp>

#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfIntAttribute.h>

#include <stddef.h>

#include <bonez/rendering/Framebuffer.hpp>

namespace BnZ {
    static std::unordered_map<std::string, Shared<Image>> s_ImageCache;

    Shared<Image> loadImage(const std::string& filepath, bool useCache) {
//        if (useCache) {
//            auto it = s_ImageCache.find(filepath);
//            if (it != end(s_ImageCache)) {
//                return (*it).second;
//            }
//        }

        int x, y, n;
        unsigned char *data = stbi_load(filepath.c_str(), &x, &y, &n, 4);
        if(!data) {
            std::cerr << "loading image " << filepath << " error: " << stbi_failure_reason() << std::endl;
            return nullptr;
        }
        Shared<Image> pImage(new Image(x, y));
        unsigned int size = x * y;
        auto scale = 1.f / 255;
        auto ptr = pImage->getPixels();
        for(auto i = 0u; i < size; ++i) {
            auto offset = 4 * i;
            ptr->r = data[offset] * scale;
            ptr->g = data[offset + 1] * scale;
            ptr->b = data[offset + 2] * scale;
            ptr->a = data[offset + 3] * scale;
            ++ptr;
        }
        stbi_image_free(data);


//        if (useCache) {
//            s_ImageCache[filepath] = pImage;
//        }

        return pImage;
    }

    void storeImage(const std::string& filepath, const Image& image) {
        Unique<unsigned char[]> data = makeUniqueArray<unsigned char>(4 * image.getPixelCount());
        unsigned char* writePtr = data.get();

        auto ptr = image.getPixels();
        for (int j = image.getHeight() - 1; j >= 0; --j) {
            for (auto i = 0u; i < image.getWidth(); ++i) {
                writePtr[0] = min(255.f, ptr->r * 255.f);
                writePtr[1] = min(255.f, ptr->g * 255.f);
                writePtr[2] = min(255.f, ptr->b * 255.f);
                writePtr[3] = min(255.f, ptr->a * 255.f);

                writePtr += 4;
                ++ptr;
            }
        }

        auto format = FilePath(filepath).ext();
        if(format == "png") {
            stbi_write_png(filepath.c_str(), image.getWidth(), image.getHeight(), 4, data.get(), 0);
        } else if(format == "bmp") {
            stbi_write_bmp(filepath.c_str(), image.getWidth(), image.getHeight(), 4, data.get());
        } else if(format == "tga") {
            stbi_write_tga(filepath.c_str(), image.getWidth(), image.getHeight(), 4, data.get());
        } else {
            std::cerr << "storeImage: Format unrecognized " << format << std::endl;
            throw std::runtime_error("storeImage: Format unrecognized");
        }
    }

    Shared<Image> loadRawImage(const std::string& filepath, bool useCache) {
        std::ifstream in(filepath, std::ios_base::binary);
        if(!in) {
            std::cerr << "Unable to open file " << filepath << std::endl;
            return nullptr;
        }

        uint32_t w;
        uint32_t h;

        in.read((char*) &w, sizeof(w));
        in.read((char*) &h, sizeof(h));

        Shared<Image> img = makeShared<Image>(w, h);

        in.read((char*) img->getPixels(), sizeof(img->getPixels()[0]) * w * h);

        if (useCache) {
            s_ImageCache[filepath] = img;
        }

        return img;
    }

    void storeRawImage(const std::string& filepath, const Image& image) {
        std::ofstream out(filepath, std::ios_base::binary);

        uint32_t w = image.getWidth();
        uint32_t h = image.getHeight();

        out.write((const char*) &w, sizeof(w));
        out.write((const char*) &h, sizeof(h));
        out.write((const char*) image.getPixels(), sizeof(image.getPixels()[0]) * w * h);
    }

    Shared<Image> loadEXRImage(const std::string& filepath, bool useCache) {
        Imf::InputFile file(filepath.c_str());

        const auto dw = file.header().dataWindow();
        const auto dx = dw.min.x;
        const auto dy = dw.min.y;
        const auto width = dw.max.x - dw.min.x + 1;
        const auto height = dw.max.y - dw.min.y + 1;

        Imf::FrameBuffer frameBuffer;

        Shared<Image> pImage = makeShared<Image>(width, height);

        const auto basePtr = (char*)(pImage->getPixels() - dx - dy * pImage->getWidth());
        const auto xStride = sizeof(Vec4f);
        const auto yStride = sizeof(Vec4f) * pImage->getWidth();
        const auto xSampling = 1;
        const auto ySampling = 1;
        const auto fillValue = 0.0;

        frameBuffer.insert("R",
                           Imf::Slice(Imf::FLOAT,
                                      basePtr + offsetof(Vec4f, x),
                                      xStride, yStride,
                                      xSampling, ySampling,
                                      fillValue));
        frameBuffer.insert("G",
                           Imf::Slice(Imf::FLOAT,
                                      basePtr + offsetof(Vec4f, y),
                                      xStride, yStride,
                                      xSampling, ySampling,
                                      fillValue));
        frameBuffer.insert("B",
                           Imf::Slice(Imf::FLOAT,
                                      basePtr + offsetof(Vec4f, z),
                                      xStride, yStride,
                                      xSampling, ySampling,
                                      fillValue));
        frameBuffer.insert("rcpWeight",
                           Imf::Slice(Imf::FLOAT,
                                      basePtr + offsetof(Vec4f, w),
                                      xStride, yStride,
                                      xSampling, ySampling,
                                      fillValue));

        file.setFrameBuffer(frameBuffer);
        file.readPixels(dw.min.y, dw.max.y);

        return pImage;
    }

    void storeEXRImage(const std::string& filepath, const Image& image) {
        Imf::Header header(image.getWidth(), image.getHeight());
        header.insert("isBnZFramebuffer", Imf::IntAttribute(0));
        header.channels().insert("R", Imf::Channel(Imf::FLOAT));
        header.channels().insert("G", Imf::Channel(Imf::FLOAT));
        header.channels().insert("B", Imf::Channel(Imf::FLOAT));
        header.channels().insert("rcpWeight", Imf::Channel(Imf::FLOAT));

        Imf::OutputFile file(filepath.c_str(), header);

        Imf::FrameBuffer frameBuffer;

        const auto basePtr = (char*) image.getPixels();
        const auto xStride = sizeof(Vec4f);
        const auto yStride = sizeof(Vec4f) * image.getWidth();

        frameBuffer.insert("R",
                           Imf::Slice(Imf::FLOAT,
                                      basePtr + offsetof(Vec4f, x),
                                      xStride, yStride));
        frameBuffer.insert("G",
                           Imf::Slice(Imf::FLOAT,
                                      basePtr + offsetof(Vec4f, y),
                                      xStride, yStride));
        frameBuffer.insert("B",
                           Imf::Slice(Imf::FLOAT,
                                      basePtr + offsetof(Vec4f, z),
                                      xStride, yStride));
        frameBuffer.insert("rcpWeight",
                           Imf::Slice(Imf::FLOAT,
                                      basePtr + offsetof(Vec4f, a),
                                      xStride, yStride));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(image.getHeight());
    }

    static bool loadEXRFramebuffer(Imf::InputFile& file, Framebuffer& framebuffer) {
        const auto dw = file.header().dataWindow();
        const auto dx = dw.min.x;
        const auto dy = dw.min.y;
        const auto width = dw.max.x - dw.min.x + 1;
        const auto height = dw.max.y - dw.min.y + 1;

        if(width != framebuffer.getWidth() || height != framebuffer.getHeight()) {
            return false;
        }

        Imf::FrameBuffer frameBuffer;

        for(auto fbChannel: range(framebuffer.getChannelCount())) {
            auto& channelName = framebuffer.getChannelName(fbChannel);

            auto& image = framebuffer.getChannel(fbChannel);

            const auto basePtr = (char*)(image.getPixels() - dx - dy * image.getWidth());
            const auto xStride = sizeof(Vec4f);
            const auto yStride = sizeof(Vec4f) * image.getWidth();
            const auto xSampling = 1;
            const auto ySampling = 1;
            const auto fillValue = 0.0;

            frameBuffer.insert(channelName + ".R",
                               Imf::Slice(Imf::FLOAT,
                                          basePtr + offsetof(Vec4f, x),
                                          xStride, yStride,
                                          xSampling, ySampling,
                                          fillValue));
            frameBuffer.insert(channelName + ".G",
                               Imf::Slice(Imf::FLOAT,
                                          basePtr + offsetof(Vec4f, y),
                                          xStride, yStride,
                                          xSampling, ySampling,
                                          fillValue));
            frameBuffer.insert(channelName + ".B",
                               Imf::Slice(Imf::FLOAT,
                                          basePtr + offsetof(Vec4f, z),
                                          xStride, yStride,
                                          xSampling, ySampling,
                                          fillValue));
            frameBuffer.insert(channelName + ".rcpWeight",
                               Imf::Slice(Imf::FLOAT,
                                          basePtr + offsetof(Vec4f, w),
                                          xStride, yStride,
                                          xSampling, ySampling,
                                          fillValue));
        }

        file.setFrameBuffer(frameBuffer);
        file.readPixels(dw.min.y, dw.max.y);

        return true;
    }

    Framebuffer loadEXRFramebuffer(const std::string& filepath) {
        Imf::InputFile file(filepath.c_str());

        auto pIsFramebufferAttribute = file.header().findTypedAttribute<Imf::IntAttribute>("isBnZFramebuffer");
        if(!pIsFramebufferAttribute || pIsFramebufferAttribute->value() != 1) {
            throw std::runtime_error("loadEXRFramebuffer: the file " + filepath + " is not a Bonez framebuffer.");
        }

        const auto dw = file.header().dataWindow();
        const auto width = dw.max.x - dw.min.x + 1;
        const auto height = dw.max.y - dw.min.y + 1;

        Framebuffer framebuffer(width, height);

        const auto& channels = file.header().channels();
        std::set<std::string> layerNames;
        channels.layers(layerNames);

        std::vector<std::string> orderedLayerNames(layerNames.size());

        for(const auto& layerName: layerNames) {
            auto pIndex = file.header().findTypedAttribute<Imf::IntAttribute>(layerName + "_index");
            if(!pIndex) {
                throw std::runtime_error("loadEXRFramebuffer: the channel " + layerName + " has no associated index.");
            }
            orderedLayerNames[pIndex->value()] = layerName;
        }

        for(const auto& channelName: orderedLayerNames) {
            framebuffer.addChannel(channelName);
        }

        if(!loadEXRFramebuffer(file, framebuffer)) {
            throw std::runtime_error("loadEXRFramebuffer: unable to load the channels");
        }
        return framebuffer;
    }

    bool loadEXRFramebuffer(const std::string& filepath, Framebuffer& framebuffer) {
        Imf::InputFile file(filepath.c_str());

        auto pIsFramebufferAttribute = file.header().findTypedAttribute<Imf::IntAttribute>("isBnZFramebuffer");
        if(!pIsFramebufferAttribute || pIsFramebufferAttribute->value() != 1) {
            throw std::runtime_error("loadEXRFramebuffer: the file " + filepath + " is not a Bonez framebuffer.");
        }

        return loadEXRFramebuffer(file, framebuffer);
    }

    void storeEXRFramebuffer(const std::string& filepath, const Framebuffer& framebuffer) {
        Imf::Header header(framebuffer.getWidth(), framebuffer.getHeight());
        header.insert("isBnZFramebuffer", Imf::IntAttribute(1));

        for(auto fbChannel: range(framebuffer.getChannelCount())) {
            auto& channelName = framebuffer.getChannelName(fbChannel);

            header.insert(channelName + "_index", Imf::IntAttribute(fbChannel));

            header.channels().insert(channelName + ".R", Imf::Channel(Imf::FLOAT));
            header.channels().insert(channelName + ".G", Imf::Channel(Imf::FLOAT));
            header.channels().insert(channelName + ".B", Imf::Channel(Imf::FLOAT));
            header.channels().insert(channelName + ".rcpWeight", Imf::Channel(Imf::FLOAT));
        }
        Imf::OutputFile file(filepath.c_str(), header);

        Imf::FrameBuffer frameBuffer;

        for(auto fbChannel: range(framebuffer.getChannelCount())) {
            auto& channelName = framebuffer.getChannelName(fbChannel);

            const auto& image = framebuffer.getChannel(fbChannel);
            const auto basePtr = (char*) image.getPixels();
            const auto xStride = sizeof(Vec4f);
            const auto yStride = sizeof(Vec4f) * image.getWidth();

            frameBuffer.insert(channelName + ".R",
                               Imf::Slice(Imf::FLOAT,
                                          basePtr + offsetof(Vec4f, x),
                                          xStride, yStride));
            frameBuffer.insert(channelName + ".G",
                               Imf::Slice(Imf::FLOAT,
                                          basePtr + offsetof(Vec4f, y),
                                          xStride, yStride));
            frameBuffer.insert(channelName + ".B",
                               Imf::Slice(Imf::FLOAT,
                                          basePtr + offsetof(Vec4f, z),
                                          xStride, yStride));
            frameBuffer.insert(channelName + ".rcpWeight",
                               Imf::Slice(Imf::FLOAT,
                                          basePtr + offsetof(Vec4f, a),
                                          xStride, yStride));
        }

        file.setFrameBuffer(frameBuffer);
        file.writePixels(framebuffer.getHeight());
    }

    Vec4f texture(const Image& image, const Vec2f& texCoords, ImageFilter filter) {
        if(filter == ImageFilter::Nearest) {
            Vec2f st = texCoords - floor(texCoords);
            auto bounds = Vec2i(image.getWidth() - 1, image.getHeight() - 1);
            Vec2i pixel = Vec2i(st * Vec2f(bounds));
            pixel = clamp(pixel, Vec2i(0), bounds);
            return image(pixel.x, pixel.y);
        } else {
            Vec2f st = texCoords - floor(texCoords);
            auto bounds = Vec2i(image.getWidth() - 1, image.getHeight() - 1);
            Vec2f imageCoords = st * Vec2f(bounds);
            Vec2f coordsInsidePixel = imageCoords - floor(imageCoords);
            Vec2i pixel = Vec2i(imageCoords);
            pixel = clamp(pixel, Vec2i(0), bounds);
            Vec4f value = zero<Vec4f>();
            float weightSum = 0.f;
            auto weight = distance(Vec2f(pixel) + Vec2f(0.5), imageCoords);
            value += weight * image(pixel.x, pixel.y);
            weightSum += weight;

            auto addNeighbourContrib = [&](int x, int y) {
                auto neighbour = Vec2i(x, y);
                neighbour = clamp(pixel, Vec2i(0), bounds);
                auto weight = distance(Vec2f(neighbour) + Vec2f(0.5), imageCoords);
                value += weight * image(neighbour.x, neighbour.y);
                weightSum += weight;
            };

            if(coordsInsidePixel.x < 0.5f) {
                if(coordsInsidePixel.y < 0.5f) {
                    addNeighbourContrib(pixel.x - 1, pixel.y);
                    addNeighbourContrib(pixel.x - 1, pixel.y - 1);
                    addNeighbourContrib(pixel.x, pixel.y - 1);
                } else {
                    addNeighbourContrib(pixel.x - 1, pixel.y);
                    addNeighbourContrib(pixel.x - 1, pixel.y + 1);
                    addNeighbourContrib(pixel.x, pixel.y + 1);
                }
            } else {
                if(coordsInsidePixel.y < 0.5f) {
                    addNeighbourContrib(pixel.x + 1, pixel.y);
                    addNeighbourContrib(pixel.x + 1, pixel.y - 1);
                    addNeighbourContrib(pixel.x, pixel.y - 1);
                } else {
                    addNeighbourContrib(pixel.x + 1, pixel.y);
                    addNeighbourContrib(pixel.x + 1, pixel.y + 1);
                    addNeighbourContrib(pixel.x, pixel.y + 1);
                }
            }

            return value / weightSum;
        }
    }

    static Vec3f computeNMSE(const Image& reference, const Image& image) {
        assert(reference.getSize() == image.getSize());

        const auto size = reference.getPixelCount();
        auto pRef = reference.getPixels();
        auto pImage = image.getPixels();

        Vec3f sumOfSquareError = zero<Vec3f>();
        Vec3f rcpScaling = zero<Vec3f>();

        bool NaNDetected = false;
        for(auto i: range(size)) {
            auto vRef = *pRef;
            auto vImage = *pImage;

            // normalize values
            if(vRef.a) vRef /= vRef.a;
            if(vImage.a) vImage /= vImage.a;

            const auto diff = Vec3f(vRef) - Vec3f(vImage);

            if(!reduceLogicalOr(BnZ::isnan(diff)) && !reduceLogicalOr(BnZ::isinf(diff))) {
                sumOfSquareError += sqr(diff);
                rcpScaling += sqr(Vec3f(vRef));
            } else {
                if(!NaNDetected) {
                    Vec2u pixel = getPixel(i, image.getSize());
                    BNZ_START_DEBUG_LOG;
                    debugLog() << "NaN or inf detected while evaluating RMSE for pixelID = " << i <<  " (x = " << pixel.x << ", y = " << pixel.y << ")" << std::endl;
                    debugLog() << "Reference value = " << *pRef << std::endl;
                    debugLog() << "Image value = " << *pImage << std::endl;
                    debugLog() << "diff = " << diff << std::endl;
                    NaNDetected = true;
                }
            }
            ++pRef;
            ++pImage;
        }

        if(NaNDetected) {
            std::cerr << "NaN or inf detected in computeRMSE. See Debug Log for more information." << std::endl;
        }

        Vec3f rmse = sumOfSquareError / rcpScaling;

        for(auto i: range(3)) {
            if(rcpScaling[i] == 0.f) {
                if(sumOfSquareError[i] == 0.f) {
                    rmse[i] = 0.f;
                } else {
                    rmse[i] = std::numeric_limits<float>::max();
                }
            }
        }

        return rmse;
    }

    Vec3f computeNormalizedRootMeanSquaredError(const Image& reference, const Image& image) {
        return sqrt(computeNMSE(reference, image));
    }

    static Vec3f computeMSE(const Image& reference, const Image& image) {
        assert(reference.getSize() == image.getSize());

        const auto size = reference.getPixelCount();
        auto pRef = reference.getPixels();
        auto pImage = image.getPixels();

        Vec3f sumOfSquareError = zero<Vec3f>();

        for(auto i: range(size)) {
            auto vRef = *pRef;
            auto vImage = *pImage;

            // normalize values
            if(vRef.a) vRef /= vRef.a;
            if(vImage.a) vImage /= vImage.a;

            const auto diff = Vec3f(vRef) - Vec3f(vImage);

            sumOfSquareError += sqr(diff);

            ++pRef;
            ++pImage;
        }

        return sumOfSquareError / float(reference.getPixelCount());
    }

    Vec3f computeRootMeanSquaredError(const Image& reference, const Image& image) {
        return sqrt(computeMSE(reference, image));
    }

    Vec3f computeMeanAbsoluteError(const Image& reference, const Image& image) {
        assert(reference.getSize() == image.getSize());

        const auto size = reference.getPixelCount();
        auto pRef = reference.getPixels();
        auto pImage = image.getPixels();

        Vec3f sumOfAbsError = zero<Vec3f>();

        for(auto i: range(size)) {
            auto vRef = *pRef;
            auto vImage = *pImage;

            // normalize values
            if(vRef.a) vRef /= vRef.a;
            if(vImage.a) vImage /= vImage.a;

            const auto diff = Vec3f(vRef) - Vec3f(vImage);

            sumOfAbsError += abs(diff);

            ++pRef;
            ++pImage;
        }

        return sumOfAbsError / float(reference.getPixelCount());
    }

    Image computeSquareErrorImage(const Image& reference, const Image& image, Vec3f& nrmse) {
        assert(reference.getSize() == image.getSize());

        Image squareErrorImage(reference.getWidth(), reference.getHeight());

        const auto size = reference.getPixelCount();
        auto pRef = reference.getPixels();
        auto pImage = image.getPixels();

        Vec3f sumOfSquareError = zero<Vec3f>();
        Vec3f rcpScaling = zero<Vec3f>();

        for(auto i = 0u; i < size; ++i) {
            auto vRef = *pRef;
            auto vImage = *pImage;

            if(vRef.a) vRef /= vRef.a;
            if(vImage.a) vImage /= vImage.a;

            const auto diff = Vec3f(vRef) - Vec3f(vImage);
            const auto sqrDiff = sqr(diff);

            const auto sqrRef = sqr(Vec3f(vRef));

            auto value = sqrDiff;

            squareErrorImage[i] = Vec4f(value, 1.f);

            sumOfSquareError += sqrDiff;
            rcpScaling += sqrRef;

            ++pRef;
            ++pImage;
        }

        Vec3f rmse = sumOfSquareError / rcpScaling;

        for(auto i: range(3)) {
            if(rcpScaling[i] == 0.f) {
                if(sumOfSquareError[i] == 0.f) {
                    rmse[i] = 0.f;
                } else {
                    rmse[i] = std::numeric_limits<float>::max();
                }
            }
        }
        nrmse = sqrt(rmse);

        return squareErrorImage;
    }

    Image computeAbsoluteErrorImage(const Image& reference, const Image& image) {
        assert(reference.getSize() == image.getSize());

        Image absoluteErrorImage(reference.getWidth(), reference.getHeight());

        const auto size = reference.getPixelCount();
        auto pRef = reference.getPixels();
        auto pImage = image.getPixels();

        for(auto i = 0u; i < size; ++i) {
            auto vRef = *pRef;
            auto vImage = *pImage;

            if(vRef.a) vRef /= vRef.a;
            if(vImage.a) vImage /= vImage.a;

            const auto diff = Vec3f(vRef) - Vec3f(vImage);
            auto value = abs(diff);

            absoluteErrorImage[i] = Vec4f(value, 1.f);

            ++pRef;
            ++pImage;
        }

        return absoluteErrorImage;
    }

    void computeImageStatistics(const Image& image, Vec3f& sum, Vec3f& mean, Vec3f& variance) {
        sum = zero<Vec3f>();
        mean = zero<Vec3f>();
        variance = zero<Vec3f>();
        for(const auto& value: image) {
            if(value.a) {
                auto tmp = Vec3f(value) / value.a;
                sum += tmp;
                variance += sqr(tmp);
            }
        }
        mean = sum / float(image.getPixelCount());
        variance = variance / float(image.getPixelCount()) - sqr(mean);
    }
}
