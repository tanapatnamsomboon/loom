-- character.lua — 3D capsule character controller + follow camera.
--
-- Attach to the Player entity. Requires, on the same entity:
--   * Rigidbody3D (Dynamic, FixedRotation = true)  — FixedRotation keeps the
--     capsule from toppling like the free capsules in physics_playground.
--   * CapsuleCollider3D
-- Scene dependency: an entity tagged "MainCamera", and the floor tagged "Floor".
--
-- Tunables — edit and save; the engine hot-reloads.
local MoveSpeed  = 5.0   -- horizontal speed (units / second)
local JumpSpeed  = 6.0   -- upward velocity applied on jump
local CameraBack = 8.0   -- how far behind the player the camera sits (+Z)
local CameraUp   = 4.0   -- how far above the player the camera sits
local CameraLerp = 8.0   -- follow smoothing — higher snaps faster

local groundContacts = 0      -- >0 while touching the floor
local jumpWasDown    = false  -- rising-edge detect for Space

-- Ground tracking via floor contacts — walls don't count, so you can't
-- wall-jump. A counter handles multiple simultaneous floor contacts.
function OnCollisionBegin(other)
    if other:GetTag() == "Floor" then groundContacts = groundContacts + 1 end
end

function OnCollisionEnd(other)
    if other:GetTag() == "Floor" then
        groundContacts = math.max(0, groundContacts - 1)
    end
end

function OnUpdate(ts)
    local vel = entity:GetLinearVelocity3D()

    -- WASD in the XZ plane (W = forward = -Z)
    local vx, vz = 0, 0
    if Input.IsKeyPressed(Key.W) then vz = vz - 1 end
    if Input.IsKeyPressed(Key.S) then vz = vz + 1 end
    if Input.IsKeyPressed(Key.A) then vx = vx - 1 end
    if Input.IsKeyPressed(Key.D) then vx = vx + 1 end
    local len = math.sqrt(vx * vx + vz * vz)
    if len > 0 then vx, vz = vx / len, vz / len end  -- no diagonal speed boost

    -- Jump on the rising edge of Space, only while grounded
    local vy       = vel.y
    local jumpDown = Input.IsKeyPressed(Key.Space)
    if jumpDown and not jumpWasDown and groundContacts > 0 then
        vy = JumpSpeed
    end
    jumpWasDown = jumpDown

    entity:SetLinearVelocity3D(Vec3(vx * MoveSpeed, vy, vz * MoveSpeed))

    -- Follow camera — ease MainCamera toward a point behind + above the player.
    -- Only the position is driven; the camera keeps its scene-set downward tilt.
    local cam = entity:FindByTag("MainCamera")
    if cam then
        local p = entity:GetTranslation()
        local c = cam:GetTranslation()
        local t = math.min(1.0, CameraLerp * ts)
        cam:SetTranslation(Vec3(
            c.x + (p.x              - c.x) * t,
            c.y + (p.y + CameraUp   - c.y) * t,
            c.z + (p.z + CameraBack - c.z) * t))
    end
end
