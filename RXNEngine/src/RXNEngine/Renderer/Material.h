#pragma once

#include "Shader.h"
#include "Texture.h"
#include <glm/glm.hpp>

namespace RXNEngine {

	class Material
	{
	public:
		struct Parameters
		{
			glm::vec4 AlbedoColor = glm::vec4(1.0f);
			glm::vec3 EmissiveColor = glm::vec3(0.0f);
			float Roughness = 0.5f;
			float Metalness = 0.0f;
			float AO = 1.0f;
			float Tiling = 1.0f;
		};

	public:
		Material(const Ref<Shader>& shader);
		virtual ~Material() = default;

		void Bind();

		void SetAlbedoColor(const glm::vec4& color) { m_Parameters.AlbedoColor = color; }
		void SetEmissiveColor(const glm::vec3& color) { m_Parameters.EmissiveColor = color; }
		void SetRoughness(float roughness) { m_Parameters.Roughness = roughness; }
		void SetMetalness(float metalness) { m_Parameters.Metalness = metalness; }
		void SetAO(float ao) { m_Parameters.AO = ao; }
		void SetTiling(float tiling) { m_Parameters.Tiling = tiling; }

		void SetAlbedoMap(const Ref<Texture2D>& texture) { m_AlbedoMap = texture; }
		void SetNormalMap(const Ref<Texture2D>& texture) { m_NormalMap = texture; }
		void SetMetalnessRoughnessMap(const Ref<Texture2D>& texture) { m_MetalnessRoughnessMap = texture; }
		void SetAOMap(const Ref<Texture2D>& texture) { m_AOMap = texture; }
		void SetEmissiveMap(const Ref<Texture2D>& texture) { m_EmissiveMap = texture; }

		Ref<Shader> GetShader() const { return m_Shader; }

		bool IsTransparent() const { return m_IsTransparent; }
		void SetTransparent(bool transparent) { m_IsTransparent = transparent; }

		const Parameters& GetParameters() const { return m_Parameters; }

	public:
		static Ref<Material> CreateDefault(const Ref<Shader>& shader);

	private:
		Ref<Shader> m_Shader;
		Parameters m_Parameters;

		Ref<Texture2D> m_AlbedoMap;
		Ref<Texture2D> m_NormalMap;
		Ref<Texture2D> m_MetalnessRoughnessMap;
		Ref<Texture2D> m_AOMap;
		Ref<Texture2D> m_EmissiveMap;

		bool m_IsTransparent = false;
	};
}