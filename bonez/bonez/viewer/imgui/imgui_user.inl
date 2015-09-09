#include <bonez/common.hpp>

namespace ImGui {
    void Value(const char* prefix, const BnZ::Vec4f& v, const char* float_format) {
        if (float_format)
        {
            char fmt[256];
            sprintf(fmt, "%%s: %s %s %s %s", float_format, float_format, float_format, float_format);
            ImGui::Text(fmt, prefix, v.x, v.y, v.z, v.w);
        }
        else
        {
            ImGui::Text("%s: %.3f %.3f %.3f %.3f", prefix, v.x, v.y, v.z, v.w);
        }
    }

    void Value(const char* prefix, const BnZ::Vec3f& v, const char* float_format) {
        if (float_format)
        {
            char fmt[256];
            sprintf(fmt, "%%s: %s %s %s", float_format, float_format, float_format);
            ImGui::Text(fmt, prefix, v.x, v.y, v.z);
        }
        else
        {
            ImGui::Text("%s: %.3f %.3f %.3f", prefix, v.x, v.y, v.z);
        }
    }

    void Value(const char* prefix, const BnZ::Vec4i& v) {
        ImGui::Text("%s: %d %d %d %d", prefix, v.x, v.y, v.z, v.z);
    }

    void Value(const char* prefix, const BnZ::Vec3i& v) {
        ImGui::Text("%s: %d %d %d", prefix, v.x, v.y, v.z);
    }

    void Value(const char* prefix, const BnZ::Vec2i& v) {
        ImGui::Text("%s: %d %d", prefix, v.x, v.y);
    }

    void Value(const char* prefix, const BnZ::Vec3u& v) {
        ImGui::Text("%s: %u %u %u", prefix, v.x, v.y, v.z);
    }

#ifdef ENVIRONMENT64
    void Value(const char* prefix, const std::size_t& v) {
        ImGui::Value(prefix, uint32_t(v));
    }
#endif

    void Value(const char* prefix, double v) {
        ImGui::Text("%s: %f", prefix, v);
    }
}

static bool InputIntN(const char* label, int* v, int components)
{
    ImGuiState* g = GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiStyle& style = g->Style;
    const float w_full = window->DC.ItemWidth.back();
    const float w_item_one = ImMax(1.0f, (float)(int)((w_full - (style.FramePadding.x*2.0f + style.ItemInnerSpacing.x) * (components - 1)) / (float)components));
    const float w_item_last = ImMax(1.0f, (float)(int)(w_full - (w_item_one + style.FramePadding.x*2.0f + style.ItemInnerSpacing.x) * (components - 1)));

    bool value_changed = false;
    ImGui::PushID(label);
    ImGui::PushItemWidth(w_item_one);
    for (int i = 0; i < components; i++)
    {
        ImGui::PushID(i);
        if (i + 1 == components)
        {
            ImGui::PopItemWidth();
            ImGui::PushItemWidth(w_item_last);
        }
        value_changed |= ImGui::InputInt("##v", &v[i], 0, 0);
        ImGui::SameLine(0, 0);
        ImGui::PopID();
    }
    ImGui::PopItemWidth();
    ImGui::PopID();

    ImGui::TextUnformatted(label, FindTextDisplayEnd(label));

    return value_changed;
}

namespace ImGui {
    bool InputInt2(const char* label, int v[2])
    {
        return InputIntN(label, v, 2);
    }

    bool InputInt3(const char* label, int v[3])
    {
        return InputIntN(label, v, 3);
    }

    bool InputInt4(const char* label, int v[4])
    {
        return InputIntN(label, v, 4);
    }
}
