-- pickup.lua — 3D sensor pickup.
--
-- Attach to each Pickup entity. Requires, on the same entity:
--   * Rigidbody3D (Static)
--   * BoxCollider3D with IsSensor = true
--
-- Vanishes when the Player enters its sensor volume.

function OnSensorBegin(other)
    if other:GetTag() ~= "Player" then return end
    Log.Info("picked up:", entity:GetTag())
    entity:Destroy()
end
