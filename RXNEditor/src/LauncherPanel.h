#pragma once
#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Project/Project.h"
#include <string>
#include <vector>
#include <functional>

namespace RXNEditor {

    class LauncherPanel
    {
    public:
        LauncherPanel();

        void OnImGuiRender();

        using ProjectLoadedCallbackFn = std::function<void(RXNEngine::Ref<RXNEngine::Project>)>;
        void SetProjectLoadedCallback(const ProjectLoadedCallbackFn& callback) { m_ProjectLoadedCallback = callback; }

        void AddRecentProject(const std::string& path);
    private:
        void LoadEditorPrefs();
        void SaveEditorPrefs();

    private:
        std::vector<std::string> m_RecentProjects;
        ProjectLoadedCallbackFn m_ProjectLoadedCallback;
    };
}