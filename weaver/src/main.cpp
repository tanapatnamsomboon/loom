#include <loom/core/application.h>
#include <loom/core/log.h>
#include "editor_layer.h"

int main() {
    Loom::Log::Init();
    LOOM_INFO("Weaver Editor initialized!");

    Loom::Application app;
    app.PushLayer(new Weaver::EditorLayer());
    app.Run();

    return 0;
}