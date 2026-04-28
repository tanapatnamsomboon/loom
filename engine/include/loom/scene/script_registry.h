#pragma once

#include "loom/core/core.h"
#include "loom/scene/scriptable_entity.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Loom {

    class LOOM_API ScriptRegistry {
    public:
        using FactoryFn = std::function<ScriptableEntity*()>;

        static void Register(const std::string& name, FactoryFn factory);

        static ScriptableEntity* Instantiate(const std::string& name);

        static const std::vector<std::string>& GetNames();

        static bool Contains(const std::string& name);

    private:
        static std::unordered_map<std::string, FactoryFn> sFactories;
        static std::vector<std::string>                   sNames;
    };

} // namespace Loom

// Place this in any .cpp that defines a ScriptableEntity subclass.
// Example:
//   LOOM_REGISTER_SCRIPT(PlayerController)
#define LOOM_REGISTER_SCRIPT(Type)                                         \
    static bool sRegistered_##Type = []() {                                \
        ::Loom::ScriptRegistry::Register(                                  \
            #Type,                                                         \
            []() -> ::Loom::ScriptableEntity* { return new Type(); }       \
        );                                                                 \
        return true;                                                       \
    }();
