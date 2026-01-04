#pragma once

#include "Shader.h"
#include "Texture.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <string>

namespace RXNEngine {

    class Material
    {
    public:
        Material(const Ref<Shader>& shader)
            : m_Shader(shader) {
        }

        virtual ~Material() = default;

        void SetInt(const std::string& name, int value) { m_UniformsInt[name] = value; }
        void SetFloat(const std::string& name, float value) { m_UniformsFloat[name] = value; }
        void SetFloat3(const std::string& name, const glm::vec3& value) { m_UniformsFloat3[name] = value; }
        void SetFloat4(const std::string& name, const glm::vec4& value) { m_UniformsFloat4[name] = value; }
        void SetMat4(const std::string& name, const glm::mat4& value) { m_UniformsMat4[name] = value; }

        void SetTexture(const std::string& name, const Ref<Texture2D>& texture) { m_Textures[name] = texture; }

        Ref<Shader> GetShader() const { return m_Shader; }

        bool IsTransparent() const { return m_IsTransparent; }
        void SetTransparent(bool transparent) { m_IsTransparent = transparent; }

    private:
        Ref<Shader> m_Shader;

        std::unordered_map<std::string, int> m_UniformsInt;
        std::unordered_map<std::string, float> m_UniformsFloat;
        std::unordered_map<std::string, glm::vec3> m_UniformsFloat3;
        std::unordered_map<std::string, glm::vec4> m_UniformsFloat4;
        std::unordered_map<std::string, glm::mat4> m_UniformsMat4;

        std::unordered_map<std::string, Ref<Texture2D>> m_Textures;
        bool m_IsTransparent = false;

        friend class Renderer;
    };
}