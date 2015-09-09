#include "GUI.hpp"

#include <bonez/image/stb_image.h>
#include <GLFW/glfw3.h>
#include <bonez/common.hpp>

#include <bonez/parsing/parsing.hpp>

namespace BnZ {
    ImguiHandle::ImguiHandle(const std::string& applicatioName, WindowManager &windowManager):
        m_IniFilename(applicatioName + ".imgui.ini"),
        m_WindowManager(windowManager) {
        m_WindowManager.onScrollEvent([](double xoffset, double yoffset) {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseWheel += (float)yoffset; // Use fractional mouse wheel, 1.0 unit 5 lines.
        });
        m_WindowManager.onKeyEvent([](int key, int scancode, int action, int mods) {
            ImGuiIO& io = ImGui::GetIO();
            if (action == GLFW_PRESS)
                io.KeysDown[key] = true;
            if (action == GLFW_RELEASE)
                io.KeysDown[key] = false;
            io.KeyCtrl = (mods & GLFW_MOD_CONTROL) != 0;
            io.KeyShift = (mods & GLFW_MOD_SHIFT) != 0;
        });
        m_WindowManager.onCharEvent([](unsigned int c) {
            if (c > 0 && c < 0x10000)
                ImGui::GetIO().AddInputCharacter((unsigned short)c);
        });

        initGL();
        initImGui();
        initStyle();
    }

    ImguiHandle::~ImguiHandle() {
        // Cleanup
        if (vao_handle) glDeleteVertexArrays(1, &vao_handle);
        if (vbo_handle) glDeleteBuffers(1, &vbo_handle);
        glDetachShader(shader_handle, vert_handle);
        glDetachShader(shader_handle, frag_handle);
        glDeleteShader(vert_handle);
        glDeleteShader(frag_handle);
        glDeleteProgram(shader_handle);

        ImGui::Shutdown();
    }

    void ImguiHandle::initGL() {
        #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))

        const GLchar *vertex_shader =
            "#version 330\n"
            "uniform mat4 ProjMtx;\n"
            "in vec2 Position;\n"
            "in vec2 UV;\n"
            "in vec4 Color;\n"
            "out vec2 Frag_UV;\n"
            "out vec4 Frag_Color;\n"
            "void main()\n"
            "{\n"
            "	Frag_UV = UV;\n"
            "	Frag_Color = Color;\n"
            "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
            "}\n";

        const GLchar* fragment_shader =
            "#version 330\n"
            "uniform sampler2D Texture;\n"
            "in vec2 Frag_UV;\n"
            "in vec4 Frag_Color;\n"
            "out vec4 Out_Color;\n"
            "void main()\n"
            "{\n"
            "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
            "}\n";

        shader_handle = glCreateProgram();
        vert_handle = glCreateShader(GL_VERTEX_SHADER);
        frag_handle = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(vert_handle, 1, &vertex_shader, 0);
        glShaderSource(frag_handle, 1, &fragment_shader, 0);
        glCompileShader(vert_handle);
        glCompileShader(frag_handle);
        glAttachShader(shader_handle, vert_handle);
        glAttachShader(shader_handle, frag_handle);
        glLinkProgram(shader_handle);

        texture_location = glGetUniformLocation(shader_handle, "Texture");
        proj_mtx_location = glGetUniformLocation(shader_handle, "ProjMtx");
        position_location = glGetAttribLocation(shader_handle, "Position");
        uv_location = glGetAttribLocation(shader_handle, "UV");
        colour_location = glGetAttribLocation(shader_handle, "Color");

        glGenBuffers(1, &vbo_handle);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
        glBufferData(GL_ARRAY_BUFFER, vbo_max_size, NULL, GL_DYNAMIC_DRAW);

        glGenVertexArrays(1, &vao_handle);
        glBindVertexArray(vao_handle);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_handle);
        glEnableVertexAttribArray(position_location);
        glEnableVertexAttribArray(uv_location);
        glEnableVertexAttribArray(colour_location);

        glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
        glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
        glVertexAttribPointer(colour_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        #undef OFFSETOF
    }

    void ImguiHandle::initImGui() {
        ImGuiIO& io = ImGui::GetIO();
        io.DeltaTime = 1.0f / 60.0f;                                  // Time elapsed since last frame, in seconds (in this sample app we'll override this every frame because our timestep is variable)
        io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;                       // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
        io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
        io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;
        io.IniFilename = m_IniFilename.c_str();

        io.UserData = this;

        io.RenderDrawListsFn = ImImpl_RenderDrawLists;
        io.SetClipboardTextFn = ImImpl_SetClipboardTextFn;
        io.GetClipboardTextFn = ImImpl_GetClipboardTextFn;

        loadFontsTexture();
    }

    void ImguiHandle::initStyle() {
        auto& style = ImGui::GetStyle();
        style.FramePadding.x = 0;
        style.FramePadding.y = 0;
        style.WindowPadding.x = 0;
        style.WindowPadding.y = 0;
        style.ItemSpacing.y = 2;
        style.WindowFillAlphaDefault = 1.f;
        style.WindowRounding = 0.f;
    }

    void ImguiHandle::loadFontsTexture() {
        ImGuiIO& io = ImGui::GetIO();
        //ImFont* my_font1 = io.Fonts->AddFontDefault();
        //ImFont* my_font2 = io.Fonts->AddFontFromFileTTF("extra_fonts/Karla-Regular.ttf", 15.0f);
        //ImFont* my_font3 = io.Fonts->AddFontFromFileTTF("extra_fonts/ProggyClean.ttf", 13.0f); my_font3->DisplayOffset.y += 1;
        //ImFont* my_font4 = io.Fonts->AddFontFromFileTTF("extra_fonts/ProggyTiny.ttf", 10.0f); my_font4->DisplayOffset.y += 1;
        //ImFont* my_font5 = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 20.0f, io.Fonts->GetGlyphRangesJapanese());

        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.

        GLuint tex_id;
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        // Store our identifier
        io.Fonts->TexID = (void *)(intptr_t)tex_id;
    }

    void ImguiHandle::updateImGui() {
        ImGuiIO& io = ImGui::GetIO();

        // Setup resolution (every frame to accommodate for window resizing)
        int w, h;
        int display_w, display_h;
        glfwGetWindowSize(m_WindowManager.getWindow(), &w, &h);
        glfwGetFramebufferSize(m_WindowManager.getWindow(), &display_w, &display_h);
        io.DisplaySize = ImVec2((float)display_w, (float)display_h);                                   // Display size, in pixels. For clamping windows positions.

        // Setup time step
        const double current_time =  glfwGetTime();
        io.DeltaTime = (float)(current_time - m_fCurrentTime);
        m_fCurrentTime = current_time;

        // Setup inputs
        // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
        double mouse_x, mouse_y;
        glfwGetCursorPos(m_WindowManager.getWindow(), &mouse_x, &mouse_y);
        mouse_x *= (float)display_w / w;                                                               // Convert mouse coordinates to pixels
        mouse_y *= (float)display_h / h;
        io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);                                          // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
        io.MouseDown[0] = m_WindowManager.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
        io.MouseDown[1] = m_WindowManager.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

        // Start the frame
        ImGui::NewFrame();
    }

    void ImguiHandle::ImImpl_RenderDrawLists(ImDrawList** const cmd_lists, int cmd_lists_count)
    {
        if (cmd_lists_count == 0)
                return;

        // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);
        glActiveTexture(GL_TEXTURE0);

        // Setup orthographic projection matrix
        const float width = ImGui::GetIO().DisplaySize.x;
        const float height = ImGui::GetIO().DisplaySize.y;
        const float ortho_projection[4][4] =
        {
            { 2.0f/width,	0.0f,			0.0f,		0.0f },
            { 0.0f,			2.0f/-height,	0.0f,		0.0f },
            { 0.0f,			0.0f,			-1.0f,		0.0f },
            { -1.0f,		1.0f,			0.0f,		1.0f },
        };

        auto pHandle = (ImguiHandle*) ImGui::GetIO().UserData;
        glUseProgram(pHandle->shader_handle);
        glUniform1i(pHandle->texture_location, 0);
        glUniformMatrix4fv(pHandle->proj_mtx_location, 1, GL_FALSE, &ortho_projection[0][0]);

        // Grow our buffer according to what we need
        size_t total_vtx_count = 0;
        for (int n = 0; n < cmd_lists_count; n++)
            total_vtx_count += cmd_lists[n]->vtx_buffer.size();
        glBindBuffer(GL_ARRAY_BUFFER, pHandle->vbo_handle);
        size_t neededBufferSize = total_vtx_count * sizeof(ImDrawVert);
        if (neededBufferSize > pHandle->vbo_max_size)
        {
            pHandle->vbo_max_size = neededBufferSize + 5000;  // Grow buffer
            glBufferData(GL_ARRAY_BUFFER, pHandle->vbo_max_size, NULL, GL_STREAM_DRAW);
        }

        // Copy and convert all vertices into a single contiguous buffer
        unsigned char* buffer_data = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        if (!buffer_data)
            return;
        for (int n = 0; n < cmd_lists_count; n++)
        {
            const ImDrawList* cmd_list = cmd_lists[n];
            memcpy(buffer_data, &cmd_list->vtx_buffer[0], cmd_list->vtx_buffer.size() * sizeof(ImDrawVert));
            buffer_data += cmd_list->vtx_buffer.size() * sizeof(ImDrawVert);
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(pHandle->vao_handle);

        int cmd_offset = 0;
        for (int n = 0; n < cmd_lists_count; n++)
        {
            const ImDrawList* cmd_list = cmd_lists[n];
            int vtx_offset = cmd_offset;
            const ImDrawCmd* pcmd_end = cmd_list->commands.end();
            for (const ImDrawCmd* pcmd = cmd_list->commands.begin(); pcmd != pcmd_end; pcmd++)
            {
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->texture_id);
                glScissor((int)pcmd->clip_rect.x, (int)(height - pcmd->clip_rect.w), (int)(pcmd->clip_rect.z - pcmd->clip_rect.x), (int)(pcmd->clip_rect.w - pcmd->clip_rect.y));
                glDrawArrays(GL_TRIANGLES, vtx_offset, pcmd->vtx_count);
                vtx_offset += pcmd->vtx_count;
            }
            cmd_offset = vtx_offset;
        }

        // Restore modified state
        glBindVertexArray(0);
        glUseProgram(0);
        glDisable(GL_SCISSOR_TEST);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
    }

    const char* ImguiHandle::ImImpl_GetClipboardTextFn()
    {
        auto pHandle = (const ImguiHandle*) ImGui::GetIO().UserData;
        return pHandle->m_WindowManager.getClipboardString();
    }

    void ImguiHandle::ImImpl_SetClipboardTextFn(const char* text)
    {
        auto pHandle = (const ImguiHandle*) ImGui::GetIO().UserData;
        pHandle->m_WindowManager.setClipboardString(text);
    }

    GUI::GUI(const std::string& applicatioName, WindowManager& windowManager):
        m_WindowManager(windowManager),
        m_IO(ImGui::GetIO()),
        m_ImguiHandle(applicatioName, windowManager) {
    }

    void GUI::loadSettings(const tinyxml2::XMLElement& documentRoot) {
        getChildAttribute(documentRoot, "GUI", m_bWindowOpenFlags);
    }

    void GUI::storeSettings(tinyxml2::XMLElement& documentRoot) const {
        setChildAttribute(documentRoot, "GUI", m_bWindowOpenFlags);
    }

    void GUI::startFrame() {
        m_ImguiHandle.updateImGui();

        if(ImGui::Begin("Windows")) {
            if(ImGui::Button("Close All")) {
                for(auto& pair: m_bWindowOpenFlags) {
                    pair.second = false;
                }
            }
            if(ImGui::Button("Open All")) {
                for(auto& pair: m_bWindowOpenFlags) {
                    pair.second = true;
                }
            }
            for(auto& pair: m_bWindowOpenFlags) {
                ImGui::Checkbox(pair.first.c_str(), &pair.second);
            }
        }
        ImGui::End();
    }

    bool GUI::draw() {
        ImGui::Render();
        return m_IO.WantCaptureMouse || m_IO.WantCaptureKeyboard;
    }

    bool GUI::isMouseClicked(MouseButton button) {
        return ImGui::IsMouseClicked(int(button));
    }

    void GUI::addSeparator() {
        ImGui::Separator();
    }

    bool GUI::addVarRW(const char* label, float& ref) {
        return ImGui::InputFloat(label, &ref);
    }

    bool GUI::addVarRW(const char* label, Vec2f& ref) {
        return ImGui::InputFloat2(label, value_ptr(ref));
    }

    bool GUI::addVarRW(const char* label, Vec3f& ref) {
        return ImGui::InputFloat3(label, value_ptr(ref));
    }

    bool GUI::addVarRW(const char* label, Vec4f& ref) {
        return ImGui::InputFloat4(label, value_ptr(ref));
    }

    bool GUI::addVarRW(const char* label, Vec2i& ref) {
        return ImGui::InputInt2(label, value_ptr(ref));
    }

    bool GUI::addVarRW(const char* label, Vec3i& ref) {
        return ImGui::InputInt3(label, value_ptr(ref));
    }

    bool GUI::addVarRW(const char* label, Vec4i& ref) {
        return ImGui::InputInt4(label, value_ptr(ref));
    }

    bool GUI::addVarRW(const char* label, bool& ref) {
        return ImGui::Checkbox(label, &ref);
    }

    bool GUI::addVarRW(const char* label, uint32_t& ref) {
        int tmp = int(ref);
        auto v = ImGui::InputInt(label, &tmp);
        ref = tmp;
        return v;
    }

#ifdef ENVIRONMENT64
    bool GUI::addVarRW(const char* label, size_t& ref) {
        int tmp = int(ref);
        auto v = ImGui::InputInt(label, &tmp);
        ref = tmp;
        return v;
    }
#endif

    bool GUI::addVarRW(const char* label, int& ref) {
        return ImGui::InputInt(label, &ref);
    }

    bool GUI::addVarRW(const char* label, char* pBuffer, size_t bufferSize) {
        return ImGui::InputText(label, pBuffer, bufferSize);
    }

    bool GUI::addVarRW(const char* label, std::string& str) {
        char buffer[4096];
        auto end = std::copy_n(begin(str), min(std::size(str), std::size(buffer) - 1), buffer);
        *end = '\0';
        auto result = addVarRW(label, buffer, std::size(buffer));
        str = buffer;
        return result;
    }

    bool GUI::addImage(const GLTexture2D& texture, size_t width, size_t height,
                       Vec2f* pMousePosInCanvas) {
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - canvas_pos.x, ImGui::GetIO().MousePos.y - canvas_pos.y);

        ImGui::Image((ImTextureID) texture.glId(), ImVec2(width, height));

        if(ImGui::IsItemHovered() && ImGui::GetWindowIsFocused()/*mouse_pos_in_canvas.x >= 0 && mouse_pos_in_canvas.y >= 0
                && mouse_pos_in_canvas.x < width && mouse_pos_in_canvas.y < height*/) {
            if(pMousePosInCanvas) {
                *pMousePosInCanvas = Vec2f(mouse_pos_in_canvas.x, mouse_pos_in_canvas.y);
            }
            return true;
        }
        return false;
    }

    static bool items_getter(void* data, int idx, const char** out_text) {
        const GUI::ItemGetFunc* pGetter = (const GUI::ItemGetFunc*)data;
        *out_text = (*pGetter)(idx);
        return true;
    }

    bool GUI::addCombo(const char* label, uint32_t& currentItem, uint32_t itemCount,
        const ItemGetFunc& getter) {
        int tmp = currentItem;
        auto v = ImGui::Combo(label, &tmp, items_getter, (void*)&getter, itemCount);
        currentItem = tmp;
        return v;
    }

    bool GUI::addCombo(const char* label, uint32_t& currentItem, uint32_t itemCount,
                       const char** labels) {
        return addCombo(label, currentItem, itemCount, [&](uint32_t idx) {
            return labels[idx];
        });
    }
}
