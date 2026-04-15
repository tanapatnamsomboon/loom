#pragma once

#include "loom/events/event.h"
#include <sstream>

namespace Loom {

    class KeyEvent : public Event {
    public:
        int GetKeyCode() const { return mKeyCode; }

        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

    protected:
        KeyEvent(int keycode) : mKeyCode(keycode) {}

    protected:
        int mKeyCode;
    };

    class KeyPressedEvent : public KeyEvent {
    public:
        KeyPressedEvent(int keycode, int repeatCount)
            : KeyEvent(keycode), mRepeatCount(repeatCount) {}

        int GetRepeatCount() const { return mRepeatCount; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "KeyPressedEvent: " << mKeyCode << " (" << mRepeatCount << " repeats)";
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyPressed)

    private:
        int mRepeatCount;
    };

    class KeyReleasedEvent : public KeyEvent {
    public:
        KeyReleasedEvent(int keycode) : KeyEvent(keycode) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "KeyReleasedEvent: " << mKeyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyReleased)
    };

    class LOOM_API KeyTypedEvent : public KeyEvent {
    public:
        KeyTypedEvent(int keycode) : KeyEvent(keycode) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "KeyTypedEvent: " << mKeyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyTyped);
    };

} // namespace Loom