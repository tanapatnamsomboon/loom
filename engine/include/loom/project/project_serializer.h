#pragma once

#include "loom/core/core.h"
#include "loom/project/project.h"
#include <memory>
#include <string>

namespace Loom {

    class LOOM_API ProjectSerializer {
    public:
        ProjectSerializer(std::shared_ptr<Project> project);

        bool Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        std::shared_ptr<Project> mProject;
    };

} // namespace Loom