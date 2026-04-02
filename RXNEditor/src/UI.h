#pragma once
#include <imgui.h>
#include <string>
#include <glm/glm.hpp>

namespace RXNEditor {

    class UI
    {
    public:
        static bool DrawFloatControl(const std::string& label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f, float columnWidth = 100.0f);
        static bool DrawIntControl(const std::string& label, int& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f, float columnWidth = 100.0f);
        static bool DrawVec2Control(const std::string& label, glm::vec2& values, float resetValue = 0.0f, float columnWidth = 100.0f);
        static bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f);
        static bool DrawColor3Control(const std::string& label, glm::vec3& values, float columnWidth = 100.0f);
        static bool DrawColor4Control(const std::string& label, glm::vec4& values, float columnWidth = 100.0f);
        static bool DrawCheckbox(const std::string& label, bool& value, float columnWidth = 100.0f);
        static bool DrawComboBox(const std::string& label, const char** options, int32_t optionCount, int32_t& selectedIndex, float columnWidth = 100.0f);
    };
}