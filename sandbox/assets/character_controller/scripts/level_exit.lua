-- level_exit.lua — sensor zone that transitions to the next scene.
--
-- Attach to the LevelExit entity. Requires, on the same entity:
--   * Rigidbody3D (Static)
--   * BoxCollider3D with IsSensor = true
--
-- When the Player steps in, queues a load of next_area.loom (the load
-- fires at end-of-frame).

function OnSensorBegin(other)
    if other:GetTag() ~= "Player" then return end
    Scene.Load("scenes/next_area.loom")
end
