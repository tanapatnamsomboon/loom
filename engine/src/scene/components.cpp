#include "loom/scene/components.h"
#include "loom/core/log.h"
#include "loom/scene/script_registry.h"

namespace Loom {

    void NativeScriptComponent::BindByName(const std::string& name) {
        if (!ScriptRegistry::Contains(name)) {
            LOOM_CORE_ERROR("NativeScriptComponent::BindByName: '{}' not found in registry.", name);
            return;
        }
        ScriptName        = name;
        InstantiateScript = [name]() { return ScriptRegistry::Instantiate(name); };
        DestroyScript     = [](NativeScriptComponent* nsc) {
            delete nsc->Instance;
            nsc->Instance = nullptr;
        };
    }

} // namespace Loom