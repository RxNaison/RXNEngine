#pragma once

#include "entt.hpp"
#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Renderer/EditorCamera.h"
#include "RXNEngine/Renderer/Framebuffer.h"

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

		void OnUpdateRuntime(float deltaTime, Ref<Framebuffer>& framebuffer);
		void OnUpdateEditor(float deltaTime, EditorCamera& camera, Ref<Framebuffer>& framebuffer);

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