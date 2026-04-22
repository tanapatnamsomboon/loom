#pragma once

#include "loom/core/core.h"
#include "loom/core/key_codes.h"
#include "loom/core/mouse_codes.h"
#include <utility>

namespace Loom {

    class LOOM_API Input {
    public:
        virtual ~Input() = default;

        static bool IsKeyPressed(Key keycode) { return sInstance->IsKeyPressedImpl(keycode); }
        static bool IsMouseButtonPressed(Mouse button) { return sInstance->IsMouseButtonPressedImpl(button); }
        static std::pair<float, float> GetMousePosition() { return sInstance->GetMousePositionImpl(); }
        static float GetMouseX() { return sInstance->GetMouseXImpl(); }
        static float GetMouseY() { return sInstance->GetMouseYImpl(); }

        static void Create();

    protected:
        virtual bool IsKeyPressedImpl(Key keycode) = 0;
        virtual bool IsMouseButtonPressedImpl(Mouse button) = 0;
        virtual std::pair<float, float> GetMousePositionImpl() = 0;
        virtual float GetMouseXImpl() = 0;
        virtual float GetMouseYImpl() = 0;

    private:
        static Input* sInstance;
    };

} // namespace Loom