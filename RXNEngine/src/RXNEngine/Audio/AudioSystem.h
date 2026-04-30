#pragma once
#include "RXNEngine/Core/Subsystem.h"
#include "AudioBackend.h"

namespace RXNEngine {

    class AudioSystem : public Subsystem
    {
    public:
        virtual void Init() override;
        virtual void Shutdown() override;

        inline AudioBackend* GetBackend() { return m_Backend.get(); }

        void LoadFMODBank(const std::string& bankFilePath);
        void UnloadFMODBank(const std::string& bankFilePath);
    private:
        Scope<AudioBackend> m_Backend;
    };
}