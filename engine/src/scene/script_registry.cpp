#include "loom/scene/script_registry.h"

namespace Loom {

    std::unordered_map<std::string, ScriptRegistry::FactoryFn> ScriptRegistry::sFactories;
    std::vector<std::string>                                   ScriptRegistry::sNames;

    void ScriptRegistry::Register(const std::string& name, FactoryFn factory) {
        if (sFactories.contains(name)) {
            LOOM_CORE_WARN("ScriptRegistry: '{}' is already registered, skipping.");
            return;
        }
        sFactories[name] = std::move(factory);
        sNames.push_back(name);
    }

    ScriptableEntity* ScriptRegistry::Instantiate(const std::string& name) {
        auto it = sFactories.find(name);
        if (it == sFactories.end()) {
            LOOM_CORE_ERROR("ScriptRegistry: '{}' is not registered.", name);
            return nullptr;
        }
        return it->second();
    }

    bool ScriptRegistry::Contains(const std::string& name) {
        return sFactories.contains(name);
    }

    const std::vector<std::string>& ScriptRegistry::GetNames() {
        return sNames;
    }

} // namespace Loom