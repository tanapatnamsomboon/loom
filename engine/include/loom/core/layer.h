#pragma once

#include "loom/core/core.h"
#include "loom/events/event.h"
#include <string>

namespace Loom {

    class LOOM_API Layer {
    public:
        Layer(const std::string& name = "Layer");
        virtual ~Layer();

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate() {}
        virtual void OnEvent(Event& event) {}

        const std::string& GetName() const { return mDebugName; }

    protected:
        std::string mDebugName;
    };

} // namespace Loom