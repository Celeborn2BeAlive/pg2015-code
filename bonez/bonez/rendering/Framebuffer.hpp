#pragma once

#include <bonez/maths/maths.hpp>
#include <bonez/image/Image.hpp>
#include <bonez/sys/threads.hpp>

namespace BnZ {

class Framebuffer {
    Vec2u m_Size;
    std::vector<Image> m_Images;
    std::vector<std::string> m_Names;
public:
    Framebuffer() = default;

    explicit Framebuffer(const Vec2u& size):
        Framebuffer(size.x, size.y) {
    }

    Framebuffer(uint32_t width, uint32_t height):
        m_Size(width, height) {
    }

    void removeChannels() {
        m_Images.clear();
        m_Names.clear();
    }

    std::size_t addChannel(const std::string& name) {
        m_Images.emplace_back(Image(m_Size.x, m_Size.y));
        m_Names.emplace_back(name);

        return m_Images.size() - std::size_t(1);
    }

    const Image& getChannel(std::size_t idx) const {
        return m_Images[idx];
    }

    void setChannel(std::size_t idx, Image image) {
        m_Images[idx] = std::move(image);
    }

    const std::string& getChannelName(std::size_t idx) const {
        return m_Names[idx];
    }

    uint32_t getWidth() const {
        return m_Size.x;
    }

    uint32_t getHeight() const {
        return m_Size.y;
    }

    std::size_t getChannelCount() const {
        return m_Images.size();
    }

    std::size_t getPixelCount() const {
        return m_Size.x * m_Size.y;
    }

    const Vec2u& getSize() const {
        return m_Size;
    }

    void accumulate(uint32_t channelIdx, uint32_t pixelIdx, const Vec4f& value) {
        if(channelIdx < m_Images.size()) {
            m_Images[channelIdx][pixelIdx] += value;
        }
    }

    // Accumulate on default channel (0)
    void accumulate(uint32_t pixelIdx, const Vec4f& value) {
        m_Images[0][pixelIdx] += value;
    }

    void clear() {
        for (auto &image: m_Images) {
            image.fill(Vec4f(0.f));
        }
    }

    void parallelClear() {
        processTasks(getPixelCount(), [this](uint32_t taskID, uint32_t threadID) {
            auto x = taskID % m_Size.x;
            auto y = taskID / m_Size.x;
            for (auto& image : m_Images) {
                image(x, y) = Vec4f(0.f);
            }
        }, getSystemThreadCount());
    }
};

}
