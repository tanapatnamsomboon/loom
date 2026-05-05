#include "runtime_layer.h"
#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/core/window.h>
#include <loom/project/project.h>
#include <loom/project/project_serializer.h>

int main(int argc, char** argv) {
    Loom::Log::Init();

    if (argc < 2) {
        LOOM_ERROR("Usage: WeaverRuntime <path/to/project.loomproj>");
        return 1;
    }

    // Deserialize the project before creating Application so we can
    // configure the window title and size from ProjectConfig.
    auto project = std::make_shared<Loom::Project>();
    Loom::ProjectSerializer serializer(project);
    if (!serializer.Deserialize(argv[1])) {
        LOOM_ERROR("WeaverRuntime: failed to load project '{}'", argv[1]);
        return 1;
    }
    Loom::Project::SetActive(project);

    const auto& config = project->GetConfig();
    std::string title  = config.WindowTitle.empty() ? config.Name : config.WindowTitle;
    Loom::WindowProps props{ title,
                             static_cast<unsigned int>(config.WindowWidth),
                             static_cast<unsigned int>(config.WindowHeight) };

    Loom::Application app(props);
    app.PushLayer(new RuntimeLayer());
    app.Run();

    return 0;
}
