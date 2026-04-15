#pragma once

#include "loom/core/input.h"

namespace Loom {

    class WindowsInput : public Input {
    protected:
        bool IsKeyPressedImpl(int keycode) override;
        bool IsMouseButtonPressedImpl(int button) override;
        std::pair<float, float> GetMousePositionImpl() override;
        float GetMouseXImpl() override;
        float GetMouseYImpl() override;
    };

} // namespace Loom