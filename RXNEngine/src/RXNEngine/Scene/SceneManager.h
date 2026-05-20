#pragma once

#include "RXNEngine/Core/Subsystem.h"
#include "RXNEngine/Scene/Scene.h"

namespace RXNEngine {

	class SceneManager : public Subsystem
	{
	public:
		virtual void Init() override;
		virtual void Update(float deltaTime) override;

		void LoadScene(const std::string& path);
		
		Ref<Scene> GetActiveScene() const { return m_ActiveScene; }
		void SetActiveScene(Ref<Scene> scene) { m_ActiveScene = scene; }

	private:
		Ref<Scene> m_ActiveScene;
		std::string m_NextScenePath;
		bool m_LoadRequested = false;
	};

}
