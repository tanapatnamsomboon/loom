#include "loom/scene/entity.h"
#include "loom/scene/components.h"

namespace Loom {

    void Entity::SetParent(Entity parent) {
        mScene->SetParent(*this, parent);
    }

    void Entity::RemoveParent() {
        mScene->RemoveParent(*this);
    }

    Entity Entity::GetParent() const {
        if (!HasComponent<RelationshipComponent>())
            return {};
        entt::entity parent = GetComponent<RelationshipComponent>().Parent;
        if (parent == entt::null)
            return {};
        return { parent, mScene };
    }

    std::vector<Entity> Entity::GetChildren() const {
        if (!HasComponent<RelationshipComponent>())
            return {};
        const auto& children = GetComponent<RelationshipComponent>().Children;
        std::vector<Entity> result;
        result.reserve(children.size());
        for (auto child : children)
            result.emplace_back(child, mScene);
        return result;
    }

} // namespace Loom
