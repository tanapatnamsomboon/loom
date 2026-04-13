#pragma once

#include "loom/events/event.h"
#include <sstream>

namespace Loom {

    class ENGINE_API WindowResizeEvent : public Event {
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

    class ENGINE_API WindowCloseEvent : public Event {
    public:
        WindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

} // namespace Loom