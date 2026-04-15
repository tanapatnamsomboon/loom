#pragma once

#include "loom/events/event.h"
#include <sstream>

namespace Loom {

    class WindowResizeEvent : public Event {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height)
            : mWidth(width), mHeight(height) {}

        unsigned int GetWidth() const { return mWidth; }
        unsigned int GetHeight() const { return mHeight; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "WindowResizeEvent: " << mWidth << ", " << mHeight;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        unsigned int mWidth, mHeight;
    };

    class WindowCloseEvent : public Event {
    public:
        WindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class WindowMovedEvent : public Event {
    public:
        WindowMovedEvent(int x, int y)
            : mWindowX(x), mWindowY(y) {}

        int GetX() const { return mWindowX; }
        int GetY() const { return mWindowY; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "WindowMovedEvent: " << mWindowX << ", " << mWindowY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowMoved)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        int mWindowX, mWindowY;
    };

    class WindowFocusEvent : public Event {
    public:
        WindowFocusEvent() = default;

        EVENT_CLASS_TYPE(WindowFocus)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class WindowLostFocusEvent : public Event {
    public:
        WindowLostFocusEvent() = default;

        EVENT_CLASS_TYPE(WindowLostFocus)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

} // namespace Loom