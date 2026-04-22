#include "platform/windows/windows_input.h"
#include "loom/core/application.h"
#include <Windows.h>

namespace Loom {

    bool WindowsInput::IsKeyPressedImpl(Key keycode) {
        return (GetAsyncKeyState((int)keycode) & 0x8000) != 0;
    }

    bool WindowsInput::IsMouseButtonPressedImpl(Mouse button) {
        return (GetAsyncKeyState((int)button) & 0x8000) != 0;
    }

    std::pair<float, float> WindowsInput::GetMousePositionImpl() {
        POINT pt;
        GetCursorPos(&pt);

        auto hwnd = (HWND)Application::Get().GetWindow().GetNativeWindow();
        ScreenToClient(hwnd, &pt);

        return { (float)pt.x, (float)pt.y };
    }

    float WindowsInput::GetMouseXImpl() {
        auto [x, y] = GetMousePositionImpl();
        return x;
    }

    float WindowsInput::GetMouseYImpl() {
        auto [x, y] = GetMousePositionImpl();
        return y;
    }

} // namespace Loom