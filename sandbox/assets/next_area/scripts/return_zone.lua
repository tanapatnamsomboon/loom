-- return_zone.lua — sensor zone that returns to the character_controller scene.
--
-- Attach to the ReturnZone entity. Requires, on the same entity:
--   * Rigidbody3D (Static)
--   * BoxCollider3D with IsSensor = true
--
-- Completes the round-trip: character_controller -> next_area -> back.

function OnSensorBegin(other)
    if other:GetTag() ~= "Player" then return end
    Scene.Load("scenes/character_controller.loom")
end
