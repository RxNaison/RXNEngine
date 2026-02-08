#pragma once

#include "RXNEngine/Renderer/Texture.h"
#include <filesystem>

namespace RXNEditor {

	class ContentBrowserPanel
	{
	public:
		ContentBrowserPanel();

		void OnImGuiRender();

	private:
		std::filesystem::path m_BaseDirectory;

		std::filesystem::path m_CurrentDirectory;

		RXNEngine::Ref<RXNEngine::Texture2D> m_DirectoryIcon;
		RXNEngine::Ref<RXNEngine::Texture2D> m_FileIcon;

		bool m_SettingsWindow;
	};

}