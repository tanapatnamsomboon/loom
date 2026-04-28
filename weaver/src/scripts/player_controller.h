#pragma once

#include <loom/scene/scriptable_entity.h>
#include <loom/scene/components.h>
#include <loom/core/input.h>

namespace Weaver {

    class PlayerController : public Loom::ScriptableEntity {
    protected:
        void OnCreate() override {

        }

        void OnUpdate(Loom::Timestep ts) override {
            auto& transform = GetComponent<Loom::TransformComponent>();
            float speed = 5.0f;

            if (Loom::Input::IsKeyPressed(Loom::Key::A)) transform.Translation.x -= speed * ts;
            if (Loom::Input::IsKeyPressed(Loom::Key::D)) transform.Translation.x += speed * ts;
            if (Loom::Input::IsKeyPressed(Loom::Key::W)) transform.Translation.y += speed * ts;
            if (Loom::Input::IsKeyPressed(Loom::Key::S)) transform.Translation.y -= speed * ts;
        }
    };

} // namespace Weaver