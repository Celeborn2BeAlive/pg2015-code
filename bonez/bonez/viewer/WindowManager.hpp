#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <bonez/types.hpp>
#include <bonez/common.hpp>

struct GLFWwindow;

namespace BnZ {
    class WindowManager {
    public:
        using ScrollCallback = std::function < void (double xoffset, double yoffset) > ;
        using KeyCallback = std::function < void(int key, int scancode, int action, int mods) > ;
        using CharCallback = std::function < void(unsigned int c) > ;

        WindowManager(uint32_t width, uint32_t height, const char* title);

        ~WindowManager();

        bool isOpen() const;

        void close();

        void swapBuffers();

        void handleEvents();

        bool hasClicked() const {
            return m_bClick;
        }

        bool isMouseButtonPressed(uint32_t button) const;

        bool isKeyPressed(int key) const;

        Vec2u getSize() const;

        float getRatio() const {
            auto size = getSize();
            return float(size.x) / size.y;
        }

        GLFWwindow* getWindow() const {
            return m_pWindow;
        }

        Vec2i getCursorPosition(const Vec2u& relativeTo = Vec2u(0)) const;

        Vec2f getCursorNDC() const;

        Vec2f getCursorNDC(const Vec4f& relativeTo) const;

        double getSeconds() const;

        const char* getClipboardString() const;

        void setClipboardString(const char* pString);

        void onScrollEvent(const ScrollCallback& callback) {
            m_ScrollCallbacks.emplace_back(callback);
        }

        void onKeyEvent(const KeyCallback& callback) {
            m_KeyCallbacks.emplace_back(callback);
        }

        void onCharEvent(const CharCallback& callback) {
            m_CharCallbacks.emplace_back(callback);
        }

    private:
        GLFWwindow* m_pWindow = nullptr;
        bool m_bButtonState = false;
        bool m_bClick = false;

        std::vector<ScrollCallback> m_ScrollCallbacks;
        std::vector<KeyCallback> m_KeyCallbacks;
        std::vector<CharCallback> m_CharCallbacks;

        static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void charCallback(GLFWwindow* window, unsigned int c);

        el::Logger* m_pLogger = el::Loggers::getLogger("WindowManager");
        el::Logger* m_pOpenGLDebugLogger = el::Loggers::getLogger("OpenGLDebug");
    };
}
