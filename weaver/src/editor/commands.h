#pragma once
#include "editor_command.h"
#include <loom/core/uuid.h>
#include <loom/scene/components.h>
#include <loom/scene/entity.h>
#include <loom/scene/scene.h>
#include <loom/scene/scene_serializer.h>
#include <functional>
#include <memory>
#include <string>

namespace Weaver {

// ---- EntityCreateCommand -----------------------------------------------
// Creates a named entity. Re-uses the same UUID on redo so downstream
// commands that reference the UUID remain valid.

class EntityCreateCommand : public IEditorCommand {
public:
    EntityCreateCommand(std::shared_ptr<Loom::Scene> scene, std::string tag,
                        uint64_t parent_uuid = 0)
        : mScene(std::move(scene)), mTag(std::move(tag)), mParentUUID(parent_uuid) {}

    void Execute() override {
        auto scene = mScene.lock();
        if (!scene) return;
        Loom::Entity e = (mCreatedUUID == 0)
            ? scene->CreateEntity(mTag)
            : scene->CreateEntityWithUUID(Loom::UUID(mCreatedUUID), mTag);
        if (mCreatedUUID == 0)
            mCreatedUUID = (uint64_t)e.GetComponent<Loom::IDComponent>().ID;
        if (mParentUUID != 0) {
            Loom::Entity parent = scene->GetEntityByUUID(Loom::UUID(mParentUUID));
            if (parent) scene->SetParent(e, parent);
        }
    }

    void Undo() override {
        auto scene = mScene.lock();
        if (!scene || mCreatedUUID == 0) return;
        Loom::Entity e = scene->GetEntityByUUID(Loom::UUID(mCreatedUUID));
        if (e) scene->DestroyEntity(e);
    }

    uint64_t GetCreatedUUID() const { return mCreatedUUID; }
    std::string GetDescription() const override { return "Create Entity '" + mTag + "'"; }

private:
    std::weak_ptr<Loom::Scene> mScene;
    std::string mTag;
    uint64_t    mParentUUID  = 0;
    uint64_t    mCreatedUUID = 0;
};

// ---- EntityDeleteCommand -----------------------------------------------
// Snapshots the entity to YAML on Execute so Undo can restore it with the
// same UUID (enabling higher commands in the stack to reference it again).

class EntityDeleteCommand : public IEditorCommand {
public:
    EntityDeleteCommand(std::shared_ptr<Loom::Scene> scene, Loom::Entity entity)
        : mScene(std::move(scene))
        , mUUID((uint64_t)entity.GetComponent<Loom::IDComponent>().ID)
        , mTag(entity.GetComponent<Loom::TagComponent>().Tag) {}

    void Execute() override {
        auto scene = mScene.lock();
        if (!scene) return;
        Loom::Entity e = scene->GetEntityByUUID(Loom::UUID(mUUID));
        if (!e) return;
        Loom::SceneSerializer ser(scene);
        mYAML = ser.SerializePrefabToString(e);
        scene->DestroyEntity(e);
    }

    void Undo() override {
        auto scene = mScene.lock();
        if (!scene || mYAML.empty()) return;
        Loom::SceneSerializer::DeserializePrefabIntoFromString(mYAML, scene.get());
    }

    std::string GetDescription() const override { return "Delete Entity '" + mTag + "'"; }

private:
    std::weak_ptr<Loom::Scene> mScene;
    uint64_t    mUUID;
    std::string mTag;
    std::string mYAML;
};

// ---- AddComponentCommand<T> --------------------------------------------

template<typename T>
class AddComponentCommand : public IEditorCommand {
public:
    AddComponentCommand(std::shared_ptr<Loom::Scene> scene, Loom::UUID entity_uuid,
                        std::string component_name)
        : mScene(std::move(scene))
        , mUUID((uint64_t)entity_uuid)
        , mComponentName(std::move(component_name)) {}

    void Execute() override {
        if (auto e = GetEntity(); e && !e.template HasComponent<T>())
            e.template AddComponent<T>();
    }

    void Undo() override {
        if (auto e = GetEntity(); e && e.template HasComponent<T>())
            e.template RemoveComponent<T>();
    }

    std::string GetDescription() const override { return "Add " + mComponentName; }

private:
    Loom::Entity GetEntity() const {
        auto s = mScene.lock();
        return s ? s->GetEntityByUUID(Loom::UUID(mUUID)) : Loom::Entity{};
    }
    std::weak_ptr<Loom::Scene> mScene;
    uint64_t    mUUID;
    std::string mComponentName;
};

// ---- RemoveComponentCommand<T> -----------------------------------------
// Captures the component's data before removal so Undo can restore it.

template<typename T>
class RemoveComponentCommand : public IEditorCommand {
public:
    RemoveComponentCommand(std::shared_ptr<Loom::Scene> scene, Loom::Entity entity,
                           std::string component_name)
        : mScene(std::move(scene))
        , mUUID((uint64_t)entity.template GetComponent<Loom::IDComponent>().ID)
        , mComponentName(std::move(component_name))
        , mSavedData(entity.template GetComponent<T>()) {}

    void Execute() override {
        if (auto e = GetEntity(); e && e.template HasComponent<T>())
            e.template RemoveComponent<T>();
    }

    void Undo() override {
        if (auto e = GetEntity(); e && !e.template HasComponent<T>())
            e.template AddComponent<T>(mSavedData);
    }

    std::string GetDescription() const override { return "Remove " + mComponentName; }

private:
    Loom::Entity GetEntity() const {
        auto s = mScene.lock();
        return s ? s->GetEntityByUUID(Loom::UUID(mUUID)) : Loom::Entity{};
    }
    std::weak_ptr<Loom::Scene> mScene;
    uint64_t    mUUID;
    std::string mComponentName;
    T           mSavedData;
};

// ---- TransformEditCommand ----------------------------------------------
// Captures a TransformComponent before/after for a single gizmo drag. The
// viewport panel snapshots the component on drag-start and pushes one
// command on drag-end so undo batches per drag, not per frame.

class TransformEditCommand : public IEditorCommand {
public:
    TransformEditCommand(std::shared_ptr<Loom::Scene> scene, Loom::UUID entity_uuid,
                         Loom::TransformComponent before, Loom::TransformComponent after,
                         std::string description = "Transform")
        : mScene(std::move(scene))
        , mUUID((uint64_t)entity_uuid)
        , mBefore(before)
        , mAfter(after)
        , mDescription(std::move(description)) {}

    void Execute() override { Apply(mAfter);  }
    void Undo()    override { Apply(mBefore); }

    std::string GetDescription() const override { return mDescription; }

private:
    void Apply(const Loom::TransformComponent& t) {
        auto s = mScene.lock();
        if (!s) return;
        Loom::Entity e = s->GetEntityByUUID(Loom::UUID(mUUID));
        if (!e || !e.HasComponent<Loom::TransformComponent>()) return;
        auto& tc = e.GetComponent<Loom::TransformComponent>();
        tc.Translation = t.Translation;
        tc.Rotation    = t.Rotation;
        tc.Scale       = t.Scale;
    }

    std::weak_ptr<Loom::Scene> mScene;
    uint64_t                   mUUID;
    Loom::TransformComponent   mBefore;
    Loom::TransformComponent   mAfter;
    std::string                mDescription;
};

// ---- PropertyEditCommand<T> --------------------------------------------
// Generic before/after for any copyable inspector property.
// Setter: void(Loom::Entity, const T&)

template<typename T>
class PropertyEditCommand : public IEditorCommand {
public:
    using Setter = std::function<void(Loom::Entity, const T&)>;

    PropertyEditCommand(std::shared_ptr<Loom::Scene> scene, Loom::UUID entity_uuid,
                        T before, T after, Setter setter, std::string description)
        : mScene(std::move(scene))
        , mUUID((uint64_t)entity_uuid)
        , mBefore(std::move(before))
        , mAfter(std::move(after))
        , mSetter(std::move(setter))
        , mDescription(std::move(description)) {}

    void Execute() override { if (auto e = GetEntity()) mSetter(e, mAfter);   }
    void Undo()    override { if (auto e = GetEntity()) mSetter(e, mBefore);  }

    std::string GetDescription() const override { return mDescription; }

private:
    Loom::Entity GetEntity() const {
        auto s = mScene.lock();
        return s ? s->GetEntityByUUID(Loom::UUID(mUUID)) : Loom::Entity{};
    }
    std::weak_ptr<Loom::Scene> mScene;
    uint64_t    mUUID;
    T           mBefore, mAfter;
    Setter      mSetter;
    std::string mDescription;
};

} // namespace Weaver
