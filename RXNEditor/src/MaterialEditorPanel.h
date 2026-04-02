#pragma once

#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Asset/Material.h"
#include <string>

namespace RXNEditor {

    class MaterialEditorPanel
    {
    public:
        MaterialEditorPanel() = default;

        void SetContext(const RXNEngine::Ref<RXNEngine::Material>& material, const std::string& filepath);

        void OnImGuiRender();

    private:
        RXNEngine::Ref<RXNEngine::Material> m_ActiveMaterial;
        std::string m_ActiveFilepath;
        bool m_IsOpen = false;
    };
}