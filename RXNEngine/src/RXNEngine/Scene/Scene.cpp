#include "rxnpch.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"
#include "RXNEngine/Renderer/Renderer.h"

namespace RXNEngine {

    Scene::Scene()
    {
    }

    Scene::~Scene()
    {
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TransformComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;

        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        m_Registry.destroy(entity);
    }

    Entity Scene::GetEntityByRay(const Ray& ray)
    {
        Entity closestEntity = {};
        float closestDistance = FLT_MAX;

        auto view = m_Registry.view<TransformComponent, MeshComponent>();
        for (auto entity : view)
        {
            auto [tc, mc] = view.get<TransformComponent, MeshComponent>(entity);
            if (!mc.ModelResource) continue;

            glm::mat4 entityTransform = tc.GetTransform();

            for (const auto& submesh : mc.ModelResource->GetSubmeshes())
            {
                glm::mat4 submeshGlobalTransform = entityTransform * submesh.LocalTransform;

                glm::mat4 inverseTransform = glm::inverse(submeshGlobalTransform);

                glm::vec3 localRayOrigin = glm::vec3(inverseTransform * glm::vec4(ray.Origin, 1.0f));
                glm::vec3 localRayDirection = glm::vec3(inverseTransform * glm::vec4(ray.Direction, 0.0f));

                Ray localRay = { localRayOrigin, localRayDirection };

                float t = 0.0f;
                if (Math::IntersectRayAABB(localRay, submesh.BoundingBox.Min, submesh.BoundingBox.Max, t))
                {
                    if (t < closestDistance && t > 0.0f)
                    {
                        closestDistance = t;
                        closestEntity = { entity, this };
                    }
                }
            }
        }

        return closestEntity;
    }

    void Scene::OnUpdateSimulation(float deltaTime)
    {
        m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc)
            {
                if (!nsc.Instance)
                {
                    nsc.Instance = nsc.InstantiateScript();

                    nsc.Instance->m_EntityHandle = entity;
                    nsc.Instance->m_Scene = this;

                    nsc.Instance->OnCreate();
                }

                nsc.Instance->OnUpdate(deltaTime);
            });
    }

    void Scene::OnRender(const Camera& camera, const glm::mat4& cameraTransform, Ref<RenderTarget>& renderTarget)
    {
        LightEnvironment lightEnv;
        {
            auto view = m_Registry.view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : view)
            {
                auto [light, transform] = view.get<DirectionalLightComponent, TransformComponent>(entity);

                glm::vec3 direction = glm::toMat3(glm::quat(transform.Rotation)) * glm::vec3(0, 0, -1);

                lightEnv.DirLight.Direction = direction;
                lightEnv.DirLight.Color = light.Color;
                lightEnv.DirLight.Intensity = light.Intensity;
            }
        }
        {
            auto group = m_Registry.group<PointLightComponent>(entt::get<TransformComponent>);
            for (auto entity : group)
            {
                auto [light, transform] = group.get<PointLightComponent, TransformComponent>(entity);
                PointLight pl;
                pl.Position = transform.Translation;
                pl.Color = light.Color;
                pl.Intensity = light.Intensity;
                pl.Radius = light.Radius;
                pl.Falloff = light.Falloff;
                lightEnv.PointLights.push_back(pl);
            }
        }

        Renderer::BeginScene(camera, cameraTransform, lightEnv, m_Skybox, renderTarget);

        auto group = m_Registry.group<TransformComponent>(entt::get<MeshComponent>);
        for (auto entity : group)
        {
            auto [transform, mesh] = group.get<TransformComponent, MeshComponent>(entity);
            if (mesh.ModelResource)
                Renderer::SubmitMesh(*mesh.ModelResource, transform.GetTransform());
        }

        if (m_Skybox)
            Renderer::DrawSkybox(m_Skybox, camera, cameraTransform);

        Renderer::EndScene();

    }

    void Scene::OnRenderEditor(float deltaTime, EditorCamera& camera, Ref<RenderTarget>& renderTarget)
    {
        LightEnvironment lightEnv;
        {
            auto view = m_Registry.view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : view)
            {
                auto [light, transform] = view.get<DirectionalLightComponent, TransformComponent>(entity);

                glm::vec3 direction = glm::toMat3(glm::quat(transform.Rotation)) * glm::vec3(0, 0, -1);

                lightEnv.DirLight.Direction = direction;
                lightEnv.DirLight.Color = light.Color;
                lightEnv.DirLight.Intensity = light.Intensity;
            }
        }
        {
            auto group = m_Registry.group<PointLightComponent>(entt::get<TransformComponent>);
            for (auto entity : group)
            {
                auto [light, transform] = group.get<PointLightComponent, TransformComponent>(entity);
                PointLight pl;
                pl.Position = transform.Translation;
                pl.Color = light.Color;
                pl.Intensity = light.Intensity;
                pl.Radius = light.Radius;
                pl.Falloff = light.Falloff;
                lightEnv.PointLights.push_back(pl);
            }
        }

        Renderer::BeginScene(camera, lightEnv, m_Skybox, renderTarget);

        auto group = m_Registry.group<TransformComponent>(entt::get<MeshComponent>);
        for (auto entity : group)
        {
            auto [transform, mesh] = group.get<TransformComponent, MeshComponent>(entity);
            if (mesh.ModelResource)
                Renderer::SubmitMesh(*mesh.ModelResource, transform.GetTransform());
        }

        if (m_Skybox)
            Renderer::DrawSkybox(m_Skybox, camera);

        Renderer::EndScene();
    }

    void Scene::OnUpdateRuntime(float deltaTime)
    {
        OnUpdateSimulation(deltaTime);
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        auto view = m_Registry.view<CameraComponent>();
        for (auto entity : view)
        {
            auto& cameraComponent = view.get<CameraComponent>(entity);
            if (!cameraComponent.FixedAspectRatio)
            {
                cameraComponent.Camera.SetViewportSize(width, height);
            }
        }
    }

    Entity Scene::GetEntityByUUID(UUID uuid)
    {
        auto view = m_Registry.view<IDComponent>();
        for (auto entity : view)
        {
            const auto& id = view.get<IDComponent>(entity);
            if (id.ID == uuid)
                return { entity, this };
        }
        return {};
    }

    Entity Scene::GetPrimaryCameraEntity()
    {
        Entity entity = GetEntityByUUID(m_PrimaryCameraID);
        if (entity && entity.HasComponent<CameraComponent>())
            return entity;

        return {};
    }

    void Scene::SetPrimaryCameraEntity(Entity entity)
    {
        if (entity.HasComponent<CameraComponent>())
        {
            m_PrimaryCameraID = entity.GetUUID();
        }
    }
}