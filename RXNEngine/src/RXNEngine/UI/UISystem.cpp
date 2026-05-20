#include "rxnpch.h"
#include "UISystem.h"
#include "RXNEngine/Scene/Scene.h"
#include "RXNEngine/Scene/Components.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Core/Input.h"
#include "RXNEngine/Scripting/ScriptEngine.h"

#include <glm/gtc/matrix_transform.hpp>

namespace RXNEngine {

    UISystem::UISystem(Scene* scene)
        : m_Scene(scene)
    {
    }

    void UISystem::Update(float deltaTime)
    {
        UpdateUITransforms();
        ProcessUIEvents();
    }

    void UISystem::CalculateUITransform(Entity entity, glm::vec2 parentMin, glm::vec2 parentMax, const glm::mat4& parentTransform, bool parentDirty)
    {
        auto& ui = entity.GetComponent<UITransformComponent>();

        bool boundsDirty = ui.IsDirty || parentDirty;

        if (boundsDirty)
        {
            glm::vec2 parentSize = parentMax - parentMin;

            ui.ComputedBoundsMin = parentMin + parentSize * ui.AnchorMin + ui.OffsetMin;
            ui.ComputedBoundsMax = parentMin + parentSize * ui.AnchorMax + ui.OffsetMax;
            ui.ComputedSize = ui.ComputedBoundsMax - ui.ComputedBoundsMin;
            ui.IsDirty = false;
        }

        glm::vec2 localCenter = (ui.ComputedBoundsMin + ui.ComputedBoundsMax) * 0.5f;
        glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(localCenter, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(ui.ComputedSize, 1.0f));
        glm::mat4 expectedTransform = parentTransform * localTransform;

        bool transformDirty = boundsDirty;
        if (ui.ComputedTransform != expectedTransform)
        {
            ui.ComputedTransform = expectedTransform;
            transformDirty = true;
        }

        if (entity.HasComponent<TransformComponent>())
            entity.GetComponent<TransformComponent>().WorldTransform = ui.ComputedTransform;

        if (entity.HasComponent<RelationshipComponent>())
        {
            auto& rc = entity.GetComponent<RelationshipComponent>();
            for (UUID childID : rc.Children)
            {
                Entity child = m_Scene->GetEntityByUUID(childID);

                if (child && child.HasComponent<UITransformComponent>())
                    CalculateUITransform(child, ui.ComputedBoundsMin, ui.ComputedBoundsMax, parentTransform, transformDirty);
            }
        }
    }

    void UISystem::UpdateUITransforms()
    {
        RXN_PROFILE_SCOPE();

        auto canvasView = m_Scene->m_Registry.view<UICanvasComponent>();
        for (auto entity : canvasView)
        {
            auto& canvas = canvasView.get<UICanvasComponent>(entity);
            if (!canvas.Active) continue;

            glm::vec2 parentMin(0.0f);
            glm::vec2 parentMax(0.0f);
            glm::mat4 parentTransform = glm::mat4(1.0f);

            if (canvas.RenderMode == CanvasRenderMode::ScreenSpaceOverlay)
            {
                parentMax = { (float)m_Scene->GetViewportWidth(), (float)m_Scene->GetViewportHeight() };
            }
            else if (canvas.RenderMode == CanvasRenderMode::WorldSpace)
            {
                parentMin = -canvas.ReferenceResolution * 0.5f;
                parentMax = canvas.ReferenceResolution * 0.5f;

                Entity e{ entity, m_Scene };
                if (e.HasComponent<TransformComponent>())
                    parentTransform = m_Scene->GetWorldTransform(e);
            }

            Entity canvasEntity = { entity, m_Scene };

            bool canvasDirty = false;
            if (canvasEntity.HasComponent<UITransformComponent>())
            {
                auto& ui = canvasEntity.GetComponent<UITransformComponent>();

                if (ui.ComputedBoundsMin != parentMin || ui.ComputedBoundsMax != parentMax || ui.IsDirty)
                {
                    canvasDirty = true;
                    ui.ComputedBoundsMin = parentMin;
                    ui.ComputedBoundsMax = parentMax;
                    ui.ComputedSize = parentMax - parentMin;
                    ui.IsDirty = false;
                }

                glm::vec2 localCenter = (ui.ComputedBoundsMin + ui.ComputedBoundsMax) * 0.5f;
                glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(localCenter, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(ui.ComputedSize, 1.0f));
                glm::mat4 expectedTransform = parentTransform * localTransform;

                if (ui.ComputedTransform != expectedTransform)
                {
                    ui.ComputedTransform = expectedTransform;
                    canvasDirty = true;
                }
            }

            if (canvasEntity.HasComponent<RelationshipComponent>())
            {
                auto& rc = canvasEntity.GetComponent<RelationshipComponent>();
                for (UUID childID : rc.Children)
                {
                    Entity child = m_Scene->GetEntityByUUID(childID);

                    if (child && child.HasComponent<UITransformComponent>())
                        CalculateUITransform(child, parentMin, parentMax, parentTransform, canvasDirty);
                }
            }
        }
    }

    void UISystem::ProcessUIEvents()
    {
        auto input = Application::Get().GetSubsystem<Input>();

        if (!input)
            return;

        auto [mx, my] = input->GetMousePosition();
        glm::vec2 mousePos = { mx, (float)m_Scene->GetViewportHeight() - my };
        
        bool isPressed = input->IsMouseButtonPressed(MouseCode::ButtonLeft);

        auto view = m_Scene->m_Registry.view<UITransformComponent, UIButtonComponent>();
        
        std::vector<entt::entity> sortedEntities(view.begin(), view.end());
        std::sort(sortedEntities.begin(), sortedEntities.end(), [&](entt::entity a, entt::entity b)
            {
                return view.get<UITransformComponent>(a).ZIndex > view.get<UITransformComponent>(b).ZIndex;
            });

        auto scriptEngine = Application::Get().GetSubsystem<ScriptEngine>();

        for (auto entity : sortedEntities)
        {
            auto& ui = view.get<UITransformComponent>(entity);
            auto& button = view.get<UIButtonComponent>(entity);
            
            Entity e{ entity, m_Scene };
            
            bool isHovered = mousePos.x >= ui.ComputedBoundsMin.x && mousePos.x <= ui.ComputedBoundsMax.x &&
                             mousePos.y >= ui.ComputedBoundsMin.y && mousePos.y <= ui.ComputedBoundsMax.y;

            ButtonState previousState = button.State;

            if (isHovered)
            {
                if (isPressed)
                    button.State = ButtonState::Pressed;
                else
                    button.State = ButtonState::Hovered;
            }
            else
            {
                button.State = ButtonState::Normal;
            }

            if (e.HasComponent<UIImageComponent>())
            {
                auto& image = e.GetComponent<UIImageComponent>();

                if (button.State == ButtonState::Normal)
                    image.TintColor = button.NormalColor;

                if (button.State == ButtonState::Hovered)
                    image.TintColor = button.HoverColor;

                if (button.State == ButtonState::Pressed)
                    image.TintColor = button.PressedColor;
            }

            if (scriptEngine && e.HasComponent<ScriptComponent>())
            {
                if (previousState == ButtonState::Normal && button.State == ButtonState::Hovered)
                    scriptEngine->InvokeMethod(e, "OnHoverEnter");
                else if (previousState != ButtonState::Normal && button.State == ButtonState::Normal)
                    scriptEngine->InvokeMethod(e, "OnHoverExit");
                
                if (previousState == ButtonState::Hovered && button.State == ButtonState::Pressed)
                    scriptEngine->InvokeMethod(e, "OnClick");
            }

            if (isHovered)
            {
                break;
            }
        }
    }

}
