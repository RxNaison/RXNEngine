#include "rxnpch.h"
#include "LauncherPanel.h"
#include "RXNEngine/Project/ProjectSerializer.h"
#include "RXNEngine/Utils/PlatformUtils.h"

#include <imgui.h>
#include <yaml-cpp/yaml.h>
#include <fstream>

using namespace RXNEngine;

namespace RXNEditor {

    LauncherPanel::LauncherPanel()
    {
        LoadEditorPrefs();
    }

    void LauncherPanel::LoadEditorPrefs()
    {
        if (std::filesystem::exists("rxn_editor_prefs.yaml"))
        {
            try
            {
                YAML::Node data = YAML::LoadFile("rxn_editor_prefs.yaml");
                if (data["RecentProjects"])
                {
                    for (auto proj : data["RecentProjects"])
                        m_RecentProjects.push_back(proj.as<std::string>());
                }
            }
            catch (...)
            {
                RXN_CORE_WARN("Failed to load editor preferences.");
            }
        }
    }

    void LauncherPanel::SaveEditorPrefs()
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
        for (const auto& proj : m_RecentProjects)
            out << proj;
        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout("rxn_editor_prefs.yaml");
        fout << out.c_str();
    }

    void LauncherPanel::AddRecentProject(const std::string& path)
    {
        auto it = std::find(m_RecentProjects.begin(), m_RecentProjects.end(), path);
        if (it != m_RecentProjects.end())
            m_RecentProjects.erase(it);

        m_RecentProjects.insert(m_RecentProjects.begin(), path);

        if (m_RecentProjects.size() > 10)
            m_RecentProjects.pop_back();

        SaveEditorPrefs();
    }

    void LauncherPanel::OnImGuiRender()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.115f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("Project Hub", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::BeginChild("Sidebar", ImVec2(250, 0), true);
        ImGui::SetCursorPosY(20.0f);

        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.3f, 1.0f), "  RXNEngine");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("    Version 1.0.0");

        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.205f, 0.21f, 1.0f));
        ImGui::Button("   Projects", ImVec2(ImGui::GetContentRegionAvail().x, 40));
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::Button("   Learn", ImVec2(ImGui::GetContentRegionAvail().x, 40));
        ImGui::Button("   Plugins", ImVec2(ImGui::GetContentRegionAvail().x, 40));
        ImGui::PopStyleColor();

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("MainContent", ImVec2(0, 0), false);
        ImGui::SetCursorPos(ImVec2(40.0f, 40.0f));

        ImGui::BeginChild("Header", ImVec2(0, 60), false);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("My Projects");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 320.0f);
        if (ImGui::Button("Open", ImVec2(150, 40)))
        {
            std::string path = FileDialogs::OpenFile("RXN Project (*.rxnproj)\0*.rxnproj\0");
            if (!path.empty())
            {
                Ref<Project> loadedProject = Project::New();
                ProjectSerializer serializer(loadedProject);
                if (serializer.Deserialize(path))
                {
                    AddRecentProject(path);
                    if (m_ProjectLoadedCallback)
                        m_ProjectLoadedCallback(loadedProject);
                }
            }
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.7f, 0.3f, 1.0f));
        if (ImGui::Button("New Project", ImVec2(150, 40)))
        {
            std::string path = FileDialogs::SaveFile("RXN Project (*.rxnproj)\0*.rxnproj\0");
            if (!path.empty())
            {
                Ref<Project> newProject = Project::New();
                newProject->GetConfig().Name = std::filesystem::path(path).stem().string();
                newProject->GetConfig().AssetDirectory = "Assets";

                std::filesystem::create_directory(std::filesystem::path(path).parent_path() / "Assets");

                ProjectSerializer serializer(newProject);
                serializer.Serialize(path);

                AddRecentProject(path);

                if (m_ProjectLoadedCallback)
                    m_ProjectLoadedCallback(newProject);
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Spacing();

        if (m_RecentProjects.empty())
        {
            ImGui::SetCursorPosX(40.0f);
            ImGui::TextDisabled("No recent projects found. Create a new one to get started!");
        }
        else
        {
            for (const auto& projectPath : m_RecentProjects)
            {
                ImGui::SetCursorPosX(40.0f);
                std::string projectName = std::filesystem::path(projectPath).stem().string();

                ImGui::PushID(projectPath.c_str());

                float btnWidth = ImGui::GetContentRegionAvail().x - 40.0f;
                if (btnWidth < 10.0f) btnWidth = 10.0f;

                ImVec2 cursorPos = ImGui::GetCursorPos();

                if (ImGui::InvisibleButton("##Row", ImVec2(btnWidth, 60.0f)))
                {
                    if (std::filesystem::exists(projectPath))
                    {
                        Ref<Project> loadedProject = Project::New();
                        ProjectSerializer serializer(loadedProject);

                        std::string safePath = projectPath;

                        if (serializer.Deserialize(safePath))
                        {
                            AddRecentProject(safePath);

                            if (m_ProjectLoadedCallback)
                                m_ProjectLoadedCallback(loadedProject);

                            ImGui::PopID();
                            break;
                        }
                    }
                }

                ImVec2 nextRowPos = ImGui::GetCursorPos();

                bool hovered = ImGui::IsItemHovered();

                ImGui::SetCursorPos(cursorPos);

                if (hovered)
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImColor(0.2f, 0.2f, 0.2f, 1.0f), 4.0f);
                }

                ImGui::SetCursorPos(ImVec2(cursorPos.x + 20.0f, cursorPos.y + 10.0f));
                ImGui::SetWindowFontScale(1.2f);
                ImGui::Text("%s", projectName.c_str());
                ImGui::SetWindowFontScale(1.0f);

                ImGui::SetCursorPos(ImVec2(cursorPos.x + 20.0f, cursorPos.y + 35.0f));
                ImGui::TextDisabled("%s", projectPath.c_str());

                ImGui::SetCursorPos(nextRowPos);
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}