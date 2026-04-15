#pragma once

#include "loom/events/event.h"
#include <sstream>

namespace Loom {

    class MouseMovedEvent : public Event {
    public:
        MouseMovedEvent(float x, float y) : mMouseX(x), mMouseY(y) {}

        float GetX() const { return mMouseX; }
        float GetY() const { return mMouseY; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "MouseMovedEvent: " << mMouseX << ", " << mMouseY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float mMouseX, mMouseY;
    };

    class MouseScrolledEvent : public Event {
    public:
        MouseScrolledEvent(float xOffset, float yOffset)
            : mXOffset(xOffset), mYOffset(yOffset) {}

        float GetXOffset() const { return mXOffset; }
        float GetYOffset() const { return mYOffset; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "MouseScrolledEvent: " << GetXOffset() << ", " << GetYOffset();
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float mXOffset, mYOffset;
    };

    class LOOM_API MouseButtonEvent : public Event {
    public:
        int GetMouseButton() const { return mButton; }

        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)

    protected:
        MouseButtonEvent(int button) : mButton(button) {}

    protected:
        int mButton;
    };

    class LOOM_API MouseButtonPressedEvent : public MouseButtonEvent {
    public:
        MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "MouseButtonPressedEvent: " << mButton;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class LOOM_API MouseButtonReleasedEvent : public MouseButtonEvent {
    public:
        MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "MouseButtonReleasedEvent: " << mButton;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };

} // namespace Loom