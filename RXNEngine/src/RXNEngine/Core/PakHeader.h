#pragma once

#include <cstdint>

namespace RXNEngine {

    struct PakHeader
    {
        char Magic[8] = { 'R', 'X', 'N', 'P', 'A', 'K', '\0', '\0' };
        uint32_t Version = 1;
        char EntryScenePath[256] = { 0 };
        char WindowTitle[256] = { 0 };
        uint32_t WindowWidth = 0;
        uint32_t WindowHeight = 0;
        uint32_t WindowMode = 0; // 0 = Windowed, 1 = Maximized, 2 = Borderless, 3 = Fullscreen
        uint32_t EntryCount = 0;
    };

}
