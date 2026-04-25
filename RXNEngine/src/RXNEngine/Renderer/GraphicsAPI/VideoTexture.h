#pragma once

#include "RXNEngine/Core/Base.h"
#include "Texture.h"

#include <vector>
#include <string>

namespace RXNEngine {

    class VideoTexture
    {
    public:
        VideoTexture(const std::string& filepath);
        ~VideoTexture();

        void Update(float deltaTime);
        Ref<Texture2D> GetTexture() const { return m_Texture; }
        const std::string& GetPath() const { return m_Path; }

        void Play() { m_IsPlaying = true; }
        void Pause() { m_IsPlaying = false; }
        void Rewind();

        void OnFrameDecoded(void* frame);

    private:
        std::string m_Path;
        void* m_Plm = nullptr;
        Ref<Texture2D> m_Texture;
        std::vector<uint8_t> m_RGBBuffer;
        bool m_IsPlaying = true;
    };
}