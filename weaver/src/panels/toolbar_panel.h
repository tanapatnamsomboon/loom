#pragma once

#include "editor_context.h"
#include <functional>

namespace Weaver {

    class ToolbarPanel {
    public:
        explicit ToolbarPanel(EditorContext& ctx);

        void SetOnPlayPressed(std::function<void()> cb) { mOnPlayPressed = std::move(cb); }
        void SetOnStopPressed(std::function<void()> cb) { mOnStopPressed = std::move(cb); }

        void OnImGuiRender();

    private:
        void RenderSettingsPopup();

        EditorContext& mContext;

        std::function<void()> mOnPlayPressed;
        std::function<void()> mOnStopPressed;
    };

} // namespace Weaver