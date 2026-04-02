#include "rxnpch.h"
#include "UI.h"
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace RXNEditor {

    bool UI::DrawFloatControl(const std::string& label, float& value, float speed, float min, float max, float columnWidth)
    {
        bool modified = false;

        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushItemWidth(-1);

        if (ImGui::DragFloat("##value", &value, speed, min, max))
            modified = true;

        ImGui::PopItemWidth();
        ImGui::Columns(1);
        ImGui::PopID();

        return modified;
    }

    bool UI::DrawIntControl(const std::string& label, int& value, float speed, float min, float max, float columnWidth)
    {
        bool modified = false;

        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushItemWidth(-1);

        if (ImGui::DragInt("##value", &value, speed, min, max))
            modified = true;

        ImGui::PopItemWidth();
        ImGui::Columns(1);
        ImGui::PopID();

        return modified;
    }

    bool UI::DrawVec2Control(const std::string& label, glm::vec2& values, float resetValue, float columnWidth)
    {
		bool modified = false;

        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

        if (ImGui::Button("X", buttonSize))
            values.x = resetValue;

        ImGui::SameLine();
        if(ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
			modified = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("Y", buttonSize))
            values.y = resetValue;

        ImGui::SameLine();
        if(ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
			modified = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();

		return modified;
    }

    bool UI::DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth)
    {
        bool modified = false;

        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

        if (ImGui::Button("X", buttonSize))
            values.x = resetValue;

        ImGui::SameLine();
        if(ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
			modified = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("Y", buttonSize))
            values.y = resetValue;

        ImGui::SameLine();
        if(ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
            modified = true;
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("Z", buttonSize))
            values.z = resetValue;

        ImGui::SameLine();
        if(ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f"))
            modified = true;
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();

		return modified;
    }

    bool UI::DrawColor3Control(const std::string& label, glm::vec3& values, float columnWidth)
    {
        bool modified = false;

        ImGui::PushID(label.c_str());
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushItemWidth(-1);
        if (ImGui::ColorEdit3("##value", glm::value_ptr(values)))
            modified = true;

        ImGui::PopItemWidth();
        ImGui::Columns(1);
        ImGui::PopID();

        return modified;
    }

    bool UI::DrawColor4Control(const std::string& label, glm::vec4& values, float columnWidth)
    {
        bool modified = false;

        ImGui::PushID(label.c_str());
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushItemWidth(-1);
        if (ImGui::ColorEdit4("##value", glm::value_ptr(values)))
            modified = true;

        ImGui::PopItemWidth();
        ImGui::Columns(1);
        ImGui::PopID();

        return modified;
    }

    bool UI::DrawCheckbox(const std::string& label, bool& value, float columnWidth)
    {
        bool modified = false;

        ImGui::PushID(label.c_str());
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushItemWidth(-1);
        if (ImGui::Checkbox("##value", &value))
            modified = true;

        ImGui::PopItemWidth();
        ImGui::Columns(1);
        ImGui::PopID();

        return modified;
    }

    bool UI::DrawComboBox(const std::string& label, const char** options, int32_t optionCount, int32_t& selectedIndex, float columnWidth)
    {
        bool modified = false;

        const char* previewValue = options[selectedIndex];

        ImGui::PushID(label.c_str());
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushItemWidth(-1);

        if (ImGui::BeginCombo("##value", previewValue))
        {
            for (int i = 0; i < optionCount; i++)
            {
                bool isSelected = (selectedIndex == i);
                if (ImGui::Selectable(options[i], isSelected))
                {
                    selectedIndex = i;
                    modified = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
        ImGui::Columns(1);
        ImGui::PopID();

        return modified;
    }
}