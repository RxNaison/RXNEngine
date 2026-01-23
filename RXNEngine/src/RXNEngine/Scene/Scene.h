#pragma once

#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Renderer/EditorCamera.h"
#include "RXNEngine/Renderer/RenderTarget.h"

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


		void OnUpdateSimulation(float deltaTime);
		void OnRender(const Camera& camera, const glm::mat4& cameraTransform, Ref<RenderTarget>& renderTarget);
		void OnRenderEditor(float deltaTime, EditorCamera& camera, Ref<RenderTarget>& renderTarget);
		void OnUpdateRuntime(float deltaTime, Ref<RenderTarget>& renderTarget);

		void OnViewportResize(uint32_t width, uint32_t height);

		Entity GetEntityByUUID(UUID uuid);

		const entt::registry& GetRaw() { return m_Registry; }
	private:
		entt::registry m_Registry;

		friend class Entity;
	};

}