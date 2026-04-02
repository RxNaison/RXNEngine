#include "rxnpch.h"
#include "ContentBrowserPanel.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Serialization/MaterialSerializer.h"

#include <imgui.h>

using namespace RXNEngine;

namespace RXNEditor {

    extern const std::filesystem::path g_AssetPath = "assets";

    ContentBrowserPanel::ContentBrowserPanel()
        : m_BaseDirectory(g_AssetPath), m_CurrentDirectory(g_AssetPath)
    {
        m_DirectoryIcon = RXNEngine::Texture2D::Create("res/icons/Directory.png");
        m_FileIcon = RXNEngine::Texture2D::Create("res/icons/File.png");

        Refresh();
    }

    void ContentBrowserPanel::Refresh()
    {
        m_CachedItems.clear();
        for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory))
        {
            ContentItem item;
            item.Path = directoryEntry.path();
            item.Filename = item.Path.filename().string();
            item.IsDirectory = directoryEntry.is_directory();
            m_CachedItems.push_back(item);
        }
    }

    void ContentBrowserPanel::DeleteItem(const std::filesystem::path& path)
    {
        try
        {
            std::filesystem::remove_all(path);
            Refresh();
        }
        catch (const std::exception& e)
        {
            RXN_CORE_ERROR("Failed to delete item: {0}", e.what());
        }
    }

    void ContentBrowserPanel::OnImGuiRender()
    {
        m_RefreshTimer += ImGui::GetIO().DeltaTime;
        if (m_RefreshTimer > m_RefreshInterval)
        {
            Refresh();
            m_RefreshTimer = 0.0f;
        }

        ImGui::Begin("Content Browser");

        DrawTopBar();

        ImGui::Separator();

        static ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("ContentBrowserLayout", 2, tableFlags))
        {
            ImGui::TableSetupColumn("Tree", ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn("Grid", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextColumn();
            ImGui::BeginChild("TreePanel");
            DrawDirectoryTree(m_BaseDirectory);
            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild("GridPanel");

            if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
            {
                if (ImGui::MenuItem("Content Browser Settings"))
                    m_SettingsWindow = true;

                ImGui::Separator();

                if (ImGui::MenuItem("Create New Material"))
                {
                    auto assetManager = Application::Get().GetSubsystem<AssetManager>();
                    Ref<Shader> defaultShader = assetManager->GetShader("res/shaders/pbr.glsl");
                    Ref<Material> newMaterial = Material::CreateDefault(defaultShader);

                    std::string newFilename = "NewMaterial.rxnmat";
                    std::filesystem::path newFilePath = m_CurrentDirectory / newFilename;

                    int count = 1;
                    while (std::filesystem::exists(newFilePath))
                    {
                        newFilename = "NewMaterial (" + std::to_string(count) + ").rxnmat";
                        newFilePath = m_CurrentDirectory / newFilename;
                        count++;
                    }

                    MaterialSerializer serializer(newMaterial);
                    serializer.Serialize(newFilePath.string());

                    RXN_CORE_INFO("Created new material at {0}", newFilePath.string());
                    Refresh();
                }

                ImGui::EndPopup();
            }

            DrawContentGrid();
            ImGui::EndChild();

            ImGui::EndTable();
        }

        if (m_SettingsWindow)
        {
            ImGui::Begin("Content Browser Settings", &m_SettingsWindow);
            ImGui::SliderFloat("Thumbnail Size", &m_ThumbnailSize, 16, 512);
            ImGui::SliderFloat("Padding", &m_Padding, 0, 32);
            ImGui::End();
        }

        ImGui::End();
    }

    void ContentBrowserPanel::DrawTopBar()
    {
        if (m_CurrentDirectory != m_BaseDirectory)
        {
            if (ImGui::Button("<- Back"))
            {
                m_CurrentDirectory = m_CurrentDirectory.parent_path();
                Refresh();
            }
            ImGui::SameLine();
        }

        ImGui::Text("%s", m_CurrentDirectory.string().c_str());

        ImGui::SameLine(ImGui::GetWindowWidth() - 250.0f);
        ImGui::SetNextItemWidth(230.0f);
        ImGui::InputTextWithHint("##Search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));
    }

    void ContentBrowserPanel::DrawDirectoryTree(const std::filesystem::path& directoryPath)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (m_CurrentDirectory == directoryPath)
            flags |= ImGuiTreeNodeFlags_Selected;

        std::string filename = directoryPath.filename().string();

        bool opened = ImGui::TreeNodeEx(directoryPath.string().c_str(), flags, "%s", filename.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            m_CurrentDirectory = directoryPath;
            Refresh();
        }

        if (opened)
        {
            for (auto& directoryEntry : std::filesystem::directory_iterator(directoryPath))
            {
                if (directoryEntry.is_directory())
                {
                    DrawDirectoryTree(directoryEntry.path());
                }
            }
            ImGui::TreePop();
        }
    }

    void ContentBrowserPanel::DrawContentGrid()
    {
        float cellSize = m_ThumbnailSize + m_Padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;

        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1) columnCount = 1;

        std::string searchString = m_SearchBuffer;
        std::transform(searchString.begin(), searchString.end(), searchString.begin(), ::tolower);

        ImGui::Columns(columnCount, 0, false);

        for (auto& item : m_CachedItems)
        {
            if (!searchString.empty())
            {
                std::string filenameLower = item.Filename;
                std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);

                if (filenameLower.find(searchString) == std::string::npos)
                    continue;
            }

            ImGui::PushID(item.Filename.c_str());

            RXNEngine::Ref<RXNEngine::Texture2D> icon = item.IsDirectory ? m_DirectoryIcon : m_FileIcon;

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::ImageButton("Button", (ImTextureID)icon->GetRendererID(), { m_ThumbnailSize, m_ThumbnailSize }, { 0, 1 }, { 1, 0 });

            if (ImGui::BeginPopupContextItem("ItemContextMenu"))
            {
                if (ImGui::MenuItem("Delete"))
                    DeleteItem(item.Path);

                if (item.Path.extension() == ".rxnmat")
                {
                    if (ImGui::MenuItem("Edit Material"))
                    {
                        if (m_MaterialOpenCallback)
                            m_MaterialOpenCallback(item.Path.string());
                    }
                }

                ImGui::EndPopup();
            }

            if (ImGui::BeginDragDropSource())
            {
                std::string itemPath = item.Path.string();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), itemPath.size() + 1);
                ImGui::Image((ImTextureID)icon->GetRendererID(), { 64, 64 }, { 0, 1 }, { 1, 0 });
                ImGui::Text("%s", item.Filename.c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    if (item.IsDirectory)
                    {
                        m_CurrentDirectory /= item.Path.filename();
                        Refresh();
                    }
                    else if (item.Path.extension() == ".rxnmat")
                    {
                        if (m_MaterialOpenCallback)
                            m_MaterialOpenCallback(item.Path.string());
                    }
                }
            }

            ImGui::TextWrapped("%s", item.Filename.c_str());

            ImGui::NextColumn();
            ImGui::PopID();
        }

        ImGui::Columns(1);
    }
}