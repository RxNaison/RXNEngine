#pragma once

#include "entt.hpp"
#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Renderer/EditorCamera.h"
#include "RXNEngine/Renderer/RenderTarget.h"

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

		void OnUpdateSimulation(float deltaTime, Ref<RenderTarget>& renderTarget);
		void OnUpdateEditor(float deltaTime, EditorCamera& camera, Ref<RenderTarget>& renderTarget);

		void OnViewportResize(uint32_t width, uint32_t height);

		// Helper to find entity by ID
		Entity GetEntityByUUID(UUID uuid);

	private:
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		friend class Entity;
		//friend class SceneHierarchyPanel; // For your UI later
	};

}