#pragma once

#include "loom/core/core.h"
#include <cstdint>
#include <functional>

namespace Loom {

    class LOOM_API UUID {
    public:
        UUID();
        UUID(uint64_t uuid);
        UUID(const UUID&) = default;

        operator uint64_t() const { return mUUID; }

    private:
        uint64_t mUUID;
    };

} // namespace Loom

namespace std {
    template <typename T> struct hash;
    template<>
    struct hash<Loom::UUID> {
        std::size_t operator()(const Loom::UUID& uuid) const {
            return (uint64_t)uuid;
        }
    };
}