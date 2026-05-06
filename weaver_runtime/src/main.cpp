#include "runtime_layer.h"
#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/core/window.h>
#include <loom/project/project.h>
#include <loom/project/project_serializer.h>
#include <filesystem>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

// Anchor the working directory to the executable's location so that relative
// engine asset paths (e.g. "resources/") resolve correctly regardless of how
// the runtime was launched (shell cwd, shortcut, double-click, etc.).
static void SetCwdToExecutableDirectory() {
#ifdef _WIN32
    wchar_t exe_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::filesystem::current_path(std::filesystem::path(exe_path).parent_path());
#endif
}

int main(int argc, char** argv) {
    Loom::Log::Init();

    if (argc < 2) {
        LOOM_ERROR("Usage: WeaverRuntime <path/to/project.loomproj>");
        return 1;
    }

    // Resolve the project path to absolute BEFORE changing the working directory
    // so that relative paths supplied by the caller are interpreted from their
    // original launch context, not from the exe directory.
    std::filesystem::path project_path = std::filesystem::absolute(argv[1]);

    SetCwdToExecutableDirectory();

    // Deserialize the project before creating Application so we can
    // configure the window title and size from ProjectConfig.
    auto project = std::make_shared<Loom::Project>();
    Loom::ProjectSerializer serializer(project);
    if (!serializer.Deserialize(project_path.string())) {
        LOOM_ERROR("WeaverRuntime: failed to load project '{}'", project_path.string());
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
