#pragma once

#include <string>
#include <vector>

#include <bonez/types.hpp>
#include <bonez/sys/memory.hpp>
#include <bonez/sys/files.hpp>

#include <bonez/opengl/utils/GLTexture.hpp>

namespace BnZ {

    class Framebuffer;

    class Image {
    public:
        typedef std::vector<Vec4f>::iterator iterator;
        typedef std::vector<Vec4f>::const_iterator const_iterator;

        Image() = default;

        Image(uint32_t w, uint32_t h, const Vec4f* pixels = nullptr) :
            m_nWidth(w), m_nHeight(h), m_Pixels(m_nWidth * m_nHeight) {
            if (pixels) {
                std::copy(pixels, pixels + m_Pixels.size(), std::begin(m_Pixels));
            }
        }
        
        uint32_t getWidth() const {
            return m_nWidth;
        }

        uint32_t getHeight() const {
            return m_nHeight;
        }

        Vec2u getSize() const {
            return Vec2u(m_nWidth, m_nHeight);
        }

        void setSize(uint32_t w, uint32_t h) {
            m_Pixels.resize(w * h);
            m_nWidth = w;
            m_nHeight = h;
        }

        void setSize(const Vec2u& size) {
            setSize(size.x, size.y);
        }

        uint32_t getPixelCount() const {
            return m_nWidth * m_nHeight;
        }

        const Vec4f* getPixels() const {
            return m_Pixels.data();
        }

        Vec4f* getPixels() {
            return m_Pixels.data();
        }

        Vec4f& operator ()(uint32_t i, uint32_t j) {
            return m_Pixels[i + j * m_nWidth];
        }

        const Vec4f& operator ()(uint32_t i, uint32_t j) const {
            return m_Pixels[i + j * m_nWidth];
        }

        const Vec4f& operator [](const Vec2u& pixel) const {
            return (*this)(pixel.x, pixel.y);
        }

        Vec4f& operator [](const Vec2u& pixel) {
            return (*this)(pixel.x, pixel.y);
        }

        const Vec4f& operator [](uint32_t pixelIndex) const {
            return m_Pixels[pixelIndex];
        }

        Vec4f& operator [](uint32_t pixelIndex) {
            return m_Pixels[pixelIndex];
        }

        void fill(const Vec4f& value) {
            std::fill(std::begin(m_Pixels), std::end(m_Pixels), value);
        }

        iterator begin() {
            return std::begin(m_Pixels);
        }

        iterator end() {
            return std::end(m_Pixels);
        }

        const_iterator begin() const {
            return std::begin(m_Pixels);
        }

        const_iterator end() const {
            return std::end(m_Pixels);
        }

        void flipY() {
            for(auto j = 0u; j <= m_nHeight / 2; ++j) {
                for(auto i = 0u; i < m_nWidth; ++i) {
                    std::swap((*this)(i, j), (*this)(i, m_nHeight - j - 1));
                }
            }
        }

        void setAlpha(float value) {
            for(auto& pixel: m_Pixels) {
                pixel.a = value;
            }
        }

        void divideByAlpha() {
            for(auto& pixel: m_Pixels) {
                if(pixel.a) {
                    pixel /= pixel.a;
                }
            }
        }

        void applyGamma(float gamma) {
            float rcpGamma = 1.f / gamma;
            for(auto& pixel: m_Pixels) {
                pixel.r = pow(pixel.r, rcpGamma);
                pixel.g = pow(pixel.g, rcpGamma);
                pixel.b = pow(pixel.b, rcpGamma);
            }
        }

        bool contains(int x, int y) const {
            return x >= 0 && y >= 0 && x < m_nWidth && y < m_nHeight;
        }

        void scale(const Vec3f& value) {
            for(auto& pixel: m_Pixels) {
                pixel *= Vec4f(value, 1.f);
            }
        }

        void scale(float value) {
            scale(Vec3f(value));
        }

        bool empty() const {
            return m_nWidth * m_nHeight == 0u;
        }

    private:
        uint32_t m_nWidth = 0, m_nHeight = 0;
        std::vector<Vec4f> m_Pixels;
    };

    inline void fillTexture(GLTexture2D& texture, const Image& image) {
        texture.setImage(0, GL_RGBA32F, image.getWidth(), image.getHeight(), 0, GL_RGBA, GL_FLOAT, image.getPixels());
    }

    inline void fillImage(Image& image, const GLTexture2D& texture) {
        image.setSize(texture.getWidth(), texture.getHeight());
        texture.getImage(0, GL_RGBA, GL_FLOAT, image.getPixels());
        image.setAlpha(1.f);
    }
    
    // Not thread safe if useCache = true
    Shared<Image> loadImage(const std::string& filepath, bool useCache = false);

    void storeImage(const std::string& filepath, const Image& image);

    Shared<Image> loadRawImage(const std::string& filepath, bool useCache = true);

    void storeRawImage(const std::string& filepath, const Image& image);

    Shared<Image> loadEXRImage(const std::string& filepath, bool useCache = true);

    void storeEXRImage(const std::string& filepath, const Image& image);

    Framebuffer loadEXRFramebuffer(const std::string& filepath);

    bool loadEXRFramebuffer(const std::string& filepath, Framebuffer& framebuffer);

    void storeEXRFramebuffer(const std::string& filepath, const Framebuffer& framebuffer);

    enum class ImageFilter {
        Nearest,
        Bilinear
    };

    Vec4f texture(const Image& image, const Vec2f& texCoords, ImageFilter filter = ImageFilter::Nearest);

    Vec3f computeNormalizedRootMeanSquaredError(const Image& reference, const Image& image);

    Vec3f computeRootMeanSquaredError(const Image& reference, const Image& image);

    Vec3f computeMeanAbsoluteError(const Image& reference, const Image& image);

    Image computeSquareErrorImage(const Image& reference, const Image& image, Vec3f& nrmse);

    Image computeAbsoluteErrorImage(const Image& reference, const Image& image);

    void computeImageStatistics(const Image& image, Vec3f& sum, Vec3f& mean, Vec3f& variance);
}
