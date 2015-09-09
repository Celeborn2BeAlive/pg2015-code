#pragma once

#include <unordered_map>

#include <bonez/opengl/utils/GLProgram.hpp>
#include <bonez/opengl/utils/GLTexture.hpp>
#include <bonez/opengl/utils/GLBuffer.hpp>
#include <bonez/opengl/utils/GLVertexArray.hpp>

#include <bonez/types.hpp>
#include <bonez/common.hpp>

#include <bonez/parsing/tinyxml/tinyxml2.h>

#include "imgui/imgui.h"
#include "WindowManager.hpp"

#define BNZ_GUI_VAR(VAR) #VAR, VAR

namespace BnZ {

class ImguiHandle {
public:
    friend class GUI;

    ImguiHandle(const std::string& applicatioName, WindowManager& windowManager);

    ~ImguiHandle();
private:
    void initGL();

    void initImGui();

    void loadFontsTexture();

    void initStyle();

    void updateImGui();

    std::string m_IniFilename;

    WindowManager& m_WindowManager;
    double m_fCurrentTime = 0;

    int shader_handle, vert_handle, frag_handle;
    int texture_location, proj_mtx_location;
    int position_location, uv_location, colour_location;
    size_t vbo_max_size = 20000;
    unsigned int vbo_handle, vao_handle;

    static void ImImpl_RenderDrawLists(ImDrawList** const cmd_lists, int cmd_lists_count);
    static const char* ImImpl_GetClipboardTextFn();
    static void ImImpl_SetClipboardTextFn(const char* text);
};

enum class MouseButton {
    LEFT,
    MIDDLE,
    RIGHT
};

class GUI {
public:
    GUI(const std::string& applicatioName, WindowManager& windowManager);

    void startFrame();

    bool draw();

    bool isMouseClicked(MouseButton button);

    void addSeparator();

    bool addVarRW(const char* label, float& ref);

    bool addVarRW(const char* label, Vec2f& ref);

    bool addVarRW(const char* label, Vec3f& ref);

    bool addVarRW(const char* label, Vec4f& ref);

    bool addVarRW(const char* label, Vec2i& ref);

    bool addVarRW(const char* label, Vec3i& ref);

    bool addVarRW(const char* label, Vec4i& ref);

    bool addVarRW(const char* label, bool& ref);

    bool addVarRW(const char* label, uint32_t& ref);

#ifdef ENVIRONMENT64
    bool addVarRW(const char* label, size_t& ref);
#endif

    bool addVarRW(const char* label, int& ref);

    bool addVarRW(const char* label, char* pBuffer, size_t bufferSize);

    bool addVarRW(const char* label, std::string& str);

    // Add an image to the current window.
    // \returns true if the mouse is inside the image and store the mouse position relative to the image in
    // *pMousePosInImage
    bool addImage(const GLTexture2D& texture, size_t width, size_t height,
                  Vec2f* pMousePosInImage = nullptr);

    bool addImage(const GLTexture2D& texture, float scale = 1.f,
                  Vec2f* pMousePosInCanvas = nullptr) {
        return addImage(texture, scale * texture.getWidth(), scale * texture.getHeight(),
                        pMousePosInCanvas);
    }

    template<typename Functor>
    bool addButton(const char* label, const Functor& f) {
        if (ImGui::Button(label)) {
            f();
            return true;
        }
        return false;
    }

    template<typename T>
    void addValue(const char* label, const T& value) {
        ImGui::Value(label, value);
    }

    void addText(const std::string& label) {
        ImGui::TextUnformatted(label.c_str());
    }

    using ItemGetFunc = std::function<const char* (uint32_t item)>;
    bool addCombo(const char* label, uint32_t& currentItem, uint32_t itemCount, const ItemGetFunc& getter);

    bool addCombo(const char* label, uint32_t& currentItem, uint32_t itemCount, const char** labels);

    template<typename T>
    bool addCombo(const char* label, T& currentItem, uint32_t itemCount, const ItemGetFunc& getter) {
        uint32_t tmp = uint32_t(currentItem);
        auto value = addCombo(label, tmp, itemCount, getter);
        currentItem = T(tmp);
        return value;
    }

    template<typename T>
    bool addCombo(const char* label, T& currentItem, uint32_t itemCount, const char** labels) {
        uint32_t tmp = uint32_t(currentItem);
        auto value = addCombo(label, tmp, itemCount, labels);
        currentItem = T(tmp);
        return value;
    }

    template<typename T, typename Functor>
    bool addRadioButtons(const char* label, T& currentItem, uint32_t itemCount, const Functor& getter) {
        int tmp = int(currentItem);
        bool value = false;
        for (auto i = 0u; i < itemCount; ++i) {
            value |= ImGui::RadioButton(getter(i), &tmp, i);
            if (i != itemCount - 1) {
                ImGui::SameLine();
            }
        }
        currentItem = T(tmp);
        return value;
    }

    template<typename T>
    bool addRadioButtons(const char* label, T& currentItem, uint32_t itemCount, const char** labels) {
        return addRadioButtons(label, currentItem, itemCount, [&](uint32_t idx) {
            return labels[idx];
        });
    }

    void sameLine() {
        ImGui::SameLine();
    }

    class WindowScope {
    public:
        WindowScope(WindowScope&& scope):
			m_bExists(scope.m_bExists),
			m_bIsCollapsed(scope.m_bIsCollapsed) {
            scope.m_bExists = false;
        }

        WindowScope& operator =(WindowScope&& scope) {
			m_bExists = scope.m_bExists;
            m_bIsCollapsed = scope.m_bIsCollapsed;
            scope.m_bExists = false;
            return *this;
        }

        ~WindowScope() {
            if (m_bExists) {
                ImGui::End();
            }
        }

        explicit operator bool() const {
            return m_bIsCollapsed;
        }

        bool isCollapsed() const {
            return m_bIsCollapsed;
        }

    private:
        friend class GUI;

        WindowScope(const char* title, bool* pIsOpen, const Vec2i& size) {
            if(*pIsOpen) {
                m_bIsCollapsed = ImGui::Begin(title, pIsOpen, ImVec2(size.x, size.y));
                m_bExists = true;
            } else {
                m_bExists = false;
            }
        }

        WindowScope(const WindowScope&) = delete;
        WindowScope& operator =(const WindowScope&) = delete;

        bool m_bExists = true;
        bool m_bIsCollapsed = false;
    };

    WindowScope addWindow(const char* title, bool isOpen = true, const Vec2i& size = Vec2u(0)) {
        auto it = m_bWindowOpenFlags.find(title);
        auto ptr = [&] {
            if(it == end(m_bWindowOpenFlags)) {
                m_bWindowOpenFlags[title] = isOpen;
                return &m_bWindowOpenFlags[title];
            } else {
                return &(*it).second;
            }
        }();

        return WindowScope(title, ptr, size);
    }

    void loadSettings(const tinyxml2::XMLElement& documentRoot);

    void storeSettings(tinyxml2::XMLElement& documentRoot) const;

private:
    WindowManager& m_WindowManager;
    double m_dTime = 0.0;

    ImGuiIO& m_IO;

    ImguiHandle m_ImguiHandle;

    std::unordered_map<std::string, bool> m_bWindowOpenFlags;
};

}
