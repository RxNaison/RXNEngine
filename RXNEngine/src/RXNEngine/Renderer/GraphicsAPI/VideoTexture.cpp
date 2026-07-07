#include "rxnpch.h"
#include "VideoTexture.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Core/VFSSystem.h"

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
        plm_t* plm = nullptr;
        
        auto vfs = Application::Get().GetSubsystem<VFSSystem>();
        if (vfs && vfs->FileExists(filepath))
        {
            m_VideoBuffer = vfs->ReadFile(filepath);
            if (!m_VideoBuffer.empty())
                plm = plm_create_with_memory(m_VideoBuffer.data(), m_VideoBuffer.size(), 0);
        }
        else
        {
            plm = plm_create_with_filename(filepath.c_str());
        }

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
        RXN_PROFILE_SCOPE_NAMED("VideoTexture Decode");
        if (!m_Plm || !m_IsPlaying)
            return;

        plm_t* plm = (plm_t*)m_Plm;
        double framerate = plm_get_framerate(plm);
        double maxStep = framerate > 0.0 ? (2.0 / framerate) : (double)deltaTime;
        plm_decode(plm, std::min((double)deltaTime, maxStep));
    }

    void VideoTexture::OnFrameDecoded(void* framePtr)
    {
        plm_frame_t* frame = (plm_frame_t*)framePtr;

        int pitch = frame->width * 3;
        plm_frame_to_rgb(frame, m_RGBBuffer.data() + (size_t)(frame->height - 1) * pitch, -pitch);

        m_Texture->SetData(m_RGBBuffer.data(), m_RGBBuffer.size());
    }
}