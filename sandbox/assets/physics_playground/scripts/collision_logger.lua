-- collision_logger.lua — logs 3D collision events.
--
-- Attach to any entity with a Rigidbody3D + a 3D collider. Each contact
-- begin/end with another body prints the other entity's tag, so you can
-- watch props settle in the console and in build/<config>/bin/logs/loom.log.

function OnCollisionBegin(other)
    Log.Info("collision begin:", entity:GetTag(), "<->", other:GetTag())
end

function OnCollisionEnd(other)
    Log.Info("collision end:", entity:GetTag(), "<->", other:GetTag())
end
