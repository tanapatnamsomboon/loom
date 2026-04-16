#pragma once

#include "loom/core/layer.h"
#include "loom/events/application_event.h"
#include "loom/events/key_event.h"
#include "loom/events/mouse_event.h"

namespace Loom {

    class LOOM_API ImGuiLayer : public Layer {
    public:
        ImGuiLayer();
        ~ImGuiLayer() override = default;

        void OnAttach() override;
        void OnDetach() override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

        void Begin();
        void End();
    };

} // namespace Loom