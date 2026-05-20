#pragma once

#include "RXNEngine/Core/Subsystem.h"
#include "RXNEngine/Scene/Entity.h"
#include <glm/glm.hpp>

namespace RXNEngine {

    class Scene;

    class UISystem : public Subsystem
    {
    public:
        UISystem(Scene* scene);
        virtual ~UISystem() = default;

        virtual void Update(float deltaTime) override;

    private:
        void UpdateUITransforms();
        void ProcessUIEvents();
        void CalculateUITransform(Entity entity, glm::vec2 parentMin, glm::vec2 parentMax, const glm::mat4& parentTransform, bool parentDirty);

    private:
        Scene* m_Scene = nullptr;
    };

}
