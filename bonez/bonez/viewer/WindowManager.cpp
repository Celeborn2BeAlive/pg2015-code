#include "WindowManager.hpp"

#include <bonez/opengl/opengl.hpp>
#include <GLFW/glfw3.h>
#include <bonez/opengl/utils/GLutils.hpp>

#include <iostream>
#include <stdexcept>

#ifndef _WIN32
    #define sprintf_s snprintf
#endif

static void formatDebugOutput(char outStr[], size_t outStrSize, GLenum source, GLenum type,
    GLuint id, GLenum severity, const char *msg)
{
    char sourceStr[32];
    const char *sourceFmt = "UNDEFINED(0x%04X)";
    switch (source)

    {
    case GL_DEBUG_SOURCE_API_ARB:             sourceFmt = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:   sourceFmt = "WINDOW_SYSTEM"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: sourceFmt = "SHADER_COMPILER"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:     sourceFmt = "THIRD_PARTY"; break;
    case GL_DEBUG_SOURCE_APPLICATION_ARB:     sourceFmt = "APPLICATION"; break;
    case GL_DEBUG_SOURCE_OTHER_ARB:           sourceFmt = "OTHER"; break;
    }

    sprintf_s(sourceStr, 32, sourceFmt, source);

    char typeStr[32];
    const char *typeFmt = "UNDEFINED(0x%04X)";
    switch (type)
    {

    case GL_DEBUG_TYPE_ERROR_ARB:               typeFmt = "ERROR"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: typeFmt = "DEPRECATED_BEHAVIOR"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:  typeFmt = "UNDEFINED_BEHAVIOR"; break;
    case GL_DEBUG_TYPE_PORTABILITY_ARB:         typeFmt = "PORTABILITY"; break;
    case GL_DEBUG_TYPE_PERFORMANCE_ARB:         typeFmt = "PERFORMANCE"; break;
    case GL_DEBUG_TYPE_OTHER_ARB:               typeFmt = "OTHER"; break;
    }
    sprintf_s(typeStr, 32, typeFmt, type);


    char severityStr[32];
    const char *severityFmt = "UNDEFINED";
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH_ARB:   severityFmt = "HIGH";   break;
    case GL_DEBUG_SEVERITY_MEDIUM_ARB: severityFmt = "MEDIUM"; break;
    case GL_DEBUG_SEVERITY_LOW_ARB:    severityFmt = "LOW"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:    severityFmt = "NOTIFICATION"; break;
    }

    sprintf_s(severityStr, 32, severityFmt, severity);

    sprintf_s(outStr, outStrSize, "OpenGL: %s [source=%s type=%s severity=%s id=%d]",
        msg, sourceStr, typeStr, severityStr, id);
}

static void debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, 
    GLsizei length, const GLchar* message, GLvoid* userParam)
{
    char finalMessage[256];
    formatDebugOutput(finalMessage, 256, source, type, id, severity, message);
    auto pLogger = (el::Logger*) userParam;
    pLogger->error(finalMessage);
}

static void errorCallback(int code, const char * desc) {
    std::cerr << "GLFW error code = " << code << " desc = " << desc << std::endl;
}

namespace BnZ {
    // GLFW callbacks
    void WindowManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        auto pWindowManager = (WindowManager*) glfwGetWindowUserPointer(window);
        for (const auto& cb : pWindowManager->m_ScrollCallbacks) {
            cb(xoffset, yoffset);
        }
    }

    void WindowManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        auto pWindowManager = (WindowManager*)glfwGetWindowUserPointer(window);
        for (const auto& cb : pWindowManager->m_KeyCallbacks) {
            cb(key, scancode, action, mods);
        }
    }

    void WindowManager::charCallback(GLFWwindow* window, unsigned int c)
    {
        auto pWindowManager = (WindowManager*)glfwGetWindowUserPointer(window);
        for (const auto& cb : pWindowManager->m_CharCallbacks) {
            cb(c);
        }
    }

    WindowManager::WindowManager(uint32_t width, uint32_t height, const char* title) {
        glfwSetErrorCallback(errorCallback);
        
        int major, minor, rev;
        glfwGetVersion(&major, &minor, &rev);

        m_pLogger->info("GLFW version %v.%v.%v", major, minor, rev);

        if (!glfwInit()) {
            throw std::runtime_error("glfwInit() error");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        if (!(m_pWindow = glfwCreateWindow(width, height, title, nullptr, nullptr))) {
            glfwTerminate();
            throw std::runtime_error("glfwCreateWindow() error");
        }
        glfwSetWindowUserPointer(m_pWindow, this);
        glfwMakeContextCurrent(m_pWindow);

        glfwSetKeyCallback(m_pWindow, keyCallback);
        glfwSetScrollCallback(m_pWindow, scrollCallback);
        glfwSetCharCallback(m_pWindow, charCallback);

        if (!BnZ::initGL()) {
            throw std::runtime_error("initGLEW() error");
        }

        m_pLogger->info("GPU Current Memory = %v", getGPUMemoryInfoCurrentAvailable());

        GLint glMajor, glMinor;
        glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
        glGetIntegerv(GL_MINOR_VERSION, &glMinor);

        m_pLogger->info("OpenGL version %v.%v", glMajor, glMinor);

        if (glewIsSupported("GL_ARB_debug_output")) {
            glDebugMessageCallbackARB((GLDEBUGPROCARB) debugCallback, m_pOpenGLDebugLogger);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
        }
        else {
            m_pLogger->warn("GL_ARB_debug_output not supported");
        }
    }

    WindowManager::~WindowManager() {
        glfwDestroyWindow(m_pWindow);
        glfwTerminate();
    }

    bool WindowManager::isOpen() const {
        return !glfwWindowShouldClose(m_pWindow);
    }

    void WindowManager::close() {
        glfwSetWindowShouldClose(m_pWindow, true);
    }

    void WindowManager::swapBuffers() {
        glfwSwapBuffers(m_pWindow);
    }

    void WindowManager::handleEvents() {
        glfwPollEvents();

        bool newState = glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_RIGHT);
        m_bClick = !newState && m_bButtonState;
        m_bButtonState = newState;
    }

    bool WindowManager::isMouseButtonPressed(uint32_t button) const {
        return glfwGetMouseButton(m_pWindow, button);
    }

    bool WindowManager::isKeyPressed(int key) const {
        return glfwGetKey(m_pWindow, key);
    }

    Vec2u WindowManager::getSize() const {
        int width, height;
        glfwGetFramebufferSize(m_pWindow, &width, &height);
        return Vec2u(width, height);
    }

    Vec2i WindowManager::getCursorPosition(const Vec2u& relativeTo) const {
        Vec2u size = getSize();
        double fMouseX, fMouseY;
        glfwGetCursorPos(m_pWindow, &fMouseX, &fMouseY);
        return Vec2i(fMouseX - relativeTo.x, size.y - 1 - std::floor(fMouseY) - relativeTo.y);
    }

    Vec2f WindowManager::getCursorNDC() const {
        Vec2u cursorPosition = getCursorPosition();
        Vec2u size = getSize();
        return Vec2f(-1.f) + 2.f * Vec2f(cursorPosition) / Vec2f(size);
    }

    Vec2f WindowManager::getCursorNDC(const Vec4f& relativeTo) const {
        Vec2u cursorPosition = getCursorPosition(Vec2f(relativeTo));
        return Vec2f(-1.f) + 2.f * Vec2f(cursorPosition) / Vec2f(relativeTo.z, relativeTo.w);
    }

    double WindowManager::getSeconds() const {
        return glfwGetTime();
    }

    const char* WindowManager::getClipboardString() const {
        return glfwGetClipboardString(m_pWindow);
    }

    void WindowManager::setClipboardString(const char* pString) {
        glfwSetClipboardString(m_pWindow, pString);
    }
}
