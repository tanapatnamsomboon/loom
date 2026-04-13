#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/events/application_event.h>
#include <loom/events/key_event.h>

int main() {
    Loom::Log::Init();
    LOOM_INFO("Sandbox initialized!");

    Loom::WindowResizeEvent w(1280, 720);
    Loom::KeyPressedEvent k(65, 1);

    LOOM_TRACE(w.ToString());
    LOOM_TRACE(k.ToString());

    Loom::Application app;
    app.Run();
    return 0;
}