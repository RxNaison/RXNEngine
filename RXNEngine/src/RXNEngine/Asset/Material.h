#pragma once
#include "RXNEngine/Renderer/GraphicsAPI/Shader.h"
#include "RXNEngine/Renderer/GraphicsAPI/Texture.h"
#include <glm/glm.hpp>
#include <string>

namespace RXNEngine {

    struct MaterialParameters
    {
        glm::vec4 AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec3 EmissiveColor = { 0.0f, 0.0f, 0.0f };
        float Roughness = 0.5f;
        float Metalness = 0.0f;
        float AO = 1.0f;
        float Tiling = 1.0f;
    };

    class Material
    {
    public:
        Material(const Ref<Shader>& shader);
        ~Material() = default;

        void Bind();
        static Ref<Material> CreateDefault(const Ref<Shader>& shader);
        static Ref<Material> Copy(const Ref<Material>& other);

        inline void SetAssetPath(const std::string& path) { m_AssetPath = path; }
        inline const std::string& GetAssetPath() const { return m_AssetPath; }

        inline MaterialParameters& GetParameters() { return m_Parameters; }
        inline void SetParameters(const MaterialParameters& params) { m_Parameters = params; }

        inline void SetTransparent(bool transparent) { m_IsTransparent = transparent; }
        inline bool IsTransparent() const { return m_IsTransparent; }

        void SetAlbedoMap(const Ref<Texture2D>& texture, const std::string& path = "") { m_AlbedoMap = texture; m_AlbedoPath = path; }
        void SetNormalMap(const Ref<Texture2D>& texture, const std::string& path = "") { m_NormalMap = texture; m_NormalPath = path; }
        void SetMetalnessMap(const Ref<Texture2D>& texture, const std::string& path = "") { m_MetalnessMap = texture; m_MetalPath = path; }
        void SetRoughnessMap(const Ref<Texture2D>& texture, const std::string& path = "") { m_RoughnessMap = texture; m_RoughPath = path; }
        void SetAOMap(const Ref<Texture2D>& texture, const std::string& path = "") { m_AOMap = texture; m_AOPath = path; }
        void SetEmissiveMap(const Ref<Texture2D>& texture, const std::string& path = "") { m_EmissiveMap = texture; m_EmissivePath = path; }

        inline Ref<Shader> GetShader() const { return m_Shader; }

        inline Ref<Texture2D> GetAlbedoMap() const { return m_AlbedoMap; }
        inline Ref<Texture2D> GetNormalMap() const { return m_NormalMap; }
        inline Ref<Texture2D> GetMetalnessMap() const { return m_MetalnessMap; }
        inline Ref<Texture2D> GetRoughnessMap() const { return m_RoughnessMap; }
        inline Ref<Texture2D> GetAOMap() const { return m_AOMap; }
        inline Ref<Texture2D> GetEmissiveMap() const { return m_EmissiveMap; }

        inline const std::string& GetAlbedoPath() const { return m_AlbedoPath; }
        inline const std::string& GetNormalPath() const { return m_NormalPath; }
        inline const std::string& GetMetalPath() const { return m_MetalPath; }
        inline const std::string& GetRoughPath() const { return m_RoughPath; }
        inline const std::string& GetAOPath() const { return m_AOPath; }
        inline const std::string& GetEmissivePath() const { return m_EmissivePath; }

    private:
        Ref<Shader> m_Shader;
        MaterialParameters m_Parameters;
        bool m_IsTransparent = false;
        std::string m_AssetPath;

        Ref<Texture2D> m_AlbedoMap;
        Ref<Texture2D> m_NormalMap;
        Ref<Texture2D> m_MetalnessMap;
        Ref<Texture2D> m_RoughnessMap;
        Ref<Texture2D> m_AOMap;
        Ref<Texture2D> m_EmissiveMap;

        std::string m_AlbedoPath;
        std::string m_NormalPath;
        std::string m_MetalPath;
        std::string m_RoughPath;
        std::string m_AOPath;
        std::string m_EmissivePath;
    };
}