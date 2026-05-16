-- Anim test: 1 -> play Idle, 2 -> play Run, S -> stop, Space -> jump to frame 0.
-- Logs every state change so you can confirm without watching the sprite.

local one_was   = false
local two_was   = false
local s_was     = false
local space_was = false

function OnUpdate(ts)
    local one_now   = Input.IsKeyPressed(Key.D1)
    local two_now   = Input.IsKeyPressed(Key.D2)
    local s_now     = Input.IsKeyPressed(Key.S)
    local space_now = Input.IsKeyPressed(Key.Space)

    if one_now and not one_was then
        entity:PlayAnimation("Idle")
        Log.Info("anim -> " .. entity:GetCurrentAnimation())
    end
    if two_now and not two_was then
        entity:PlayAnimation("Run")
        Log.Info("anim -> " .. entity:GetCurrentAnimation())
    end
    if s_now and not s_was then
        entity:StopAnimation()
        Log.Info("stopped (playing=" .. tostring(entity:IsAnimationPlaying()) .. ")")
    end
    if space_now and not space_was then
        entity:SetAnimationFrame(0)
        Log.Info("jumped to frame " .. entity:GetAnimationFrame())
    end

    one_was   = one_now
    two_was   = two_now
    s_was     = s_now
    space_was = space_now
end
