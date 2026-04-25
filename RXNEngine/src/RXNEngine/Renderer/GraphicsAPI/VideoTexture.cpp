#include "rxnpch.h"
#include "VideoTexture.h"

#pragma warning(push, 0) 
#define PL_MPEG_IMPLEMENTATION
#include <pl_mpeg.h>
#pragma warning(pop)

namespace RXNEngine {

    static void VideoFrameCallback(plm_t* mpeg, plm_frame_t* frame, void* user)
    {
        ((VideoTexture*)user)->OnFrameDecoded((void*)frame);
    }

    VideoTexture::VideoTexture(const std::string& filepath) : m_Path(filepath)
    {
        plm_t* plm = plm_create_with_filename(filepath.c_str());
        m_Plm = (void*)plm;

        if (!m_Plm)
        {
            RXN_CORE_ERROR("Failed to load video file: {0}", filepath);
            return;
        }

        plm_set_video_decode_callback(plm, VideoFrameCallback, this);
        plm_set_loop(plm, 1);
        plm_set_audio_enabled(plm, 0);

        int width = plm_get_width(plm);
        int height = plm_get_height(plm);
        m_RGBBuffer.resize(width * height * 3);

        TextureSpecification spec;
        spec.Width = width;
        spec.Height = height;
        spec.Format = ImageFormat::RGB8;
        spec.GenerateMips = false;
        m_Texture = Texture2D::Create(spec);

        plm_decode(plm, 0.0);
    }

    VideoTexture::~VideoTexture()
    {
        if (m_Plm)
            plm_destroy((plm_t*)m_Plm);
    }

    void VideoTexture::Rewind()
    {
        if (m_Plm)
            plm_rewind((plm_t*)m_Plm);
    }

    void VideoTexture::Update(float deltaTime)
    {
        if (m_Plm && m_IsPlaying)
            plm_decode((plm_t*)m_Plm, deltaTime);
    }

    void VideoTexture::OnFrameDecoded(void* framePtr)
    {
        plm_frame_t* frame = (plm_frame_t*)framePtr;

        plm_frame_to_rgb(frame, m_RGBBuffer.data(), frame->width * 3);

        int pitch = frame->width * 3;
        std::vector<uint8_t> tempRow(pitch);

        for (int y = 0; y < frame->height / 2; ++y)
        {
            uint8_t* rowTop = m_RGBBuffer.data() + y * pitch;
            uint8_t* rowBottom = m_RGBBuffer.data() + (frame->height - 1 - y) * pitch;

            std::memcpy(tempRow.data(), rowTop, pitch);
            std::memcpy(rowTop, rowBottom, pitch);
            std::memcpy(rowBottom, tempRow.data(), pitch);
        }

        m_Texture->SetData(m_RGBBuffer.data(), m_RGBBuffer.size());
    }
}