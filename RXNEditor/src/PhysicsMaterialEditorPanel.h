#pragma once
#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Asset/PhysicsMaterial.h"
#include <string>

namespace RXNEditor {

    class PhysicsMaterialEditorPanel
    {
    public:
        PhysicsMaterialEditorPanel() = default;

        void SetContext(const RXNEngine::Ref<RXNEngine::PhysicsMaterial>& material, const std::string& filepath);
        void OnImGuiRender();

    private:
        RXNEngine::Ref<RXNEngine::PhysicsMaterial> m_ActiveMaterial;
        std::string m_ActiveFilepath;
        bool m_IsOpen = false;
    };
}