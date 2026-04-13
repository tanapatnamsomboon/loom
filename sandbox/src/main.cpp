#include <loom/core/application.h>
#include <loom/core/log.h>

int main() {
    Loom::Log::Init();
    LOOM_INFO("Sandbox initialized!");

    Loom::Application app;
    app.Run();
    return 0;
}