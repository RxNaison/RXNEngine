#pragma once

#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Renderer/EditorCamera.h"
#include "RXNEngine/Renderer/RenderTarget.h"
#include "RXNEngine/Core/Math.h"
#include "RXNEngine/Renderer/Texture.h"

#include <entt.hpp>

namespace RXNEngine {

	class Entity;

	class Scene
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		Entity GetEntityByRay(const Ray& ray);

		void OnUpdateSimulation(float deltaTime);
		void OnRender(const Camera& camera, const glm::mat4& cameraTransform, Ref<RenderTarget>& renderTarget);
		void OnRenderEditor(float deltaTime, EditorCamera& camera, Ref<RenderTarget>& renderTarget);
		void OnUpdateRuntime(float deltaTime);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnViewportResize(uint32_t width, uint32_t height);

		Entity GetEntityByUUID(UUID uuid);

		Entity GetPrimaryCameraEntity();
		void SetPrimaryCameraEntity(Entity entity);

		void SetSkybox(Ref<Cubemap>& skybox) { m_Skybox = skybox; }

		const entt::registry& GetRaw() { return m_Registry; }
	private:
		entt::registry m_Registry;
		UUID m_PrimaryCameraID = UUID::Null;

		Ref<Cubemap> m_Skybox = nullptr;
		float m_SkyboxIntensity = 1.0f;

		friend class Entity;
		friend class SceneSerializer;
	};

}