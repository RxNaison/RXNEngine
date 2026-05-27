#include "rxnpch.h"
#include "SceneManager.h"
#include "RXNEngine/Serialization/SceneSerializer.h"
#include "RXNEngine/Core/Application.h"

namespace RXNEngine {

	void SceneManager::Init()
	{
	}

	void SceneManager::Shutdown()
	{
		if (m_ActiveScene)
		{
			m_ActiveScene->OnRuntimeStop();
			m_ActiveScene = nullptr;
		}
	}

	void SceneManager::Update(float deltaTime)
	{
		if (m_LoadRequested)
		{
			if (m_ActiveScene)
				m_ActiveScene->OnRuntimeStop();

			Ref<Scene> newScene = CreateRef<Scene>();
			SceneSerializer serializer(newScene);
			
			bool success = false;
			if (m_NextScenePath.find(".rxnbin") != std::string::npos)
				success = serializer.DeserializeRuntime(m_NextScenePath);
			else
				success = serializer.Deserialize(m_NextScenePath);

			if (success)
			{
				m_ActiveScene = newScene;
				
				uint32_t width = Application::Get().GetWindow().GetWidth();
				uint32_t height = Application::Get().GetWindow().GetHeight();
				m_ActiveScene->OnViewportResize(width, height);
				
				m_ActiveScene->OnRuntimeStart();
			}
			else
			{
				RXN_CORE_ERROR("SceneManager: Failed to load scene {0}", m_NextScenePath);
			}

			m_LoadRequested = false;
			m_NextScenePath.clear();
		}
	}

	void SceneManager::LoadScene(const std::string& path)
	{
		m_NextScenePath = path;
		m_LoadRequested = true;
	}

}
