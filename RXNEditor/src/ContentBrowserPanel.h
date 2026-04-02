#pragma once
#include <filesystem>
#include <vector>
#include "RXNEngine/Renderer/GraphicsAPI/Texture.h"

namespace RXNEditor {

    struct ContentItem
    {
        std::filesystem::path Path;
        std::string Filename;
        bool IsDirectory;
    };

    class ContentBrowserPanel
    {
    public:
        ContentBrowserPanel();
        void OnImGuiRender();

        using MaterialOpenCallbackFn = std::function<void(const std::string&)>;
        void SetMaterialOpenCallback(const MaterialOpenCallbackFn& callback) { m_MaterialOpenCallback = callback; }

    private:
        void DrawTopBar();
        void DrawDirectoryTree(const std::filesystem::path& directoryPath);
        void DrawContentGrid();

        void Refresh();
        void DeleteItem(const std::filesystem::path& path);

    private:
        std::filesystem::path m_BaseDirectory;
        std::filesystem::path m_CurrentDirectory;

        RXNEngine::Ref<RXNEngine::Texture2D> m_DirectoryIcon;
        RXNEngine::Ref<RXNEngine::Texture2D> m_FileIcon;

        std::vector<ContentItem> m_CachedItems;
        float m_RefreshTimer = 0.0f;
        float m_RefreshInterval = 0.5f;

        char m_SearchBuffer[256] = "";

        float m_ThumbnailSize = 96.0f;
        float m_Padding = 16.0f;
        bool m_SettingsWindow = false;

        MaterialOpenCallbackFn m_MaterialOpenCallback;
    };
}