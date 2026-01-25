#pragma once

#include "RXNEngine/Renderer/Model.h"
#include <fstream>

namespace RXNEngine {

    class ModelSerializer
    {
    public:
        static void Serialize(const std::string& filepath, const Model& model);

        static bool Deserialize(const std::string& filepath, Model& outModel);
    };
}