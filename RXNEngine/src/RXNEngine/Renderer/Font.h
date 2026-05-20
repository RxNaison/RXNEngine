#pragma once

#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Renderer/GraphicsAPI/Texture.h"

#include <filesystem>
#include <string>

namespace RXNEngine {

	struct MSDFData;

	class Font
	{
	public:
		Font(const std::filesystem::path& filepath);
		~Font();

		Ref<Texture2D> GetAtlasTexture() const { return m_AtlasTexture; }
		MSDFData* GetMSDFData() const { return m_Data; }
		const std::filesystem::path& GetPath() const { return m_Path; }

		static void Init();
		static void Shutdown();

	private:
		MSDFData* m_Data = nullptr;
		std::filesystem::path m_Path;
		Ref<Texture2D> m_AtlasTexture;
	};
}
