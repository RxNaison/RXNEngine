#pragma once

#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Scene/EditorCamera.h"
#include "RXNEngine/Renderer/RenderTarget.h"
#include "RXNEngine/Math/Math.h"
#include "RXNEngine/Renderer/GraphicsAPI/Texture.h"

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

		void ParentEntity(Entity entity, Entity parent);
		bool IsDescendantOf(Entity entity, Entity potentialAscendant);
		glm::mat4 GetWorldTransform(Entity entity);

		void OnUpdateSimulation(float deltaTime);
		void OnRender(const Camera& camera, const glm::mat4& cameraTransform, Ref<RenderTarget>& renderTarget, bool showColliders);
		void OnRenderEditor(float deltaTime, EditorCamera& camera, Ref<RenderTarget>& renderTarget, bool showColliders);
		void OnUpdateRuntime(float deltaTime);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnSimulationStart();
		void OnSimulationStop();

		void SyncTransformToPhysics(Entity entity);

		void OnViewportResize(uint32_t width, uint32_t height);

		Entity GetEntityByUUID(UUID uuid);
		Entity DuplicateEntity(Entity entity);
		Entity FindEntityByName(std::string_view name);

		Entity GetPrimaryCameraEntity();
		void SetPrimaryCameraEntity(Entity entity);

		void SetSkybox(Ref<Cubemap>& skybox) { m_Skybox = skybox; }

		const entt::registry& GetRaw() { return m_Registry; }

		static Ref<Scene> Copy(Ref<Scene> other);

		bool IsRunning() { return m_IsRunning; }
		bool IsSimulating() { return m_IsSimulating; }

	private:
		void UpdateWorldTransforms();
		void OnCameraComponentAdded(entt::registry& registry, entt::entity entity);
		void RemoveEntity(Entity entity);
		void CreatePhysicsBody(Entity entity);
	private:
		entt::registry m_Registry;
		UUID m_PrimaryCameraID = UUID::Null;

		Ref<Cubemap> m_Skybox = nullptr;
		float m_SkyboxIntensity = 1.0f;

		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;

		bool m_IsRunning = false;
		bool m_IsSimulating = false;

		std::unordered_map<UUID, entt::entity> m_EntityMap;

		std::vector<Entity> m_EntitiesToDestroy;

		friend class Entity;
		friend class SceneSerializer;
	};

}