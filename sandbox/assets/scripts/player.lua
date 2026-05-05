local is_grounded = false
local speed = 1.0

function OnUpdate(ts)
    local vel = entity:GetLinearVelocity()

    if Input.IsKeyPressed(Key.A) then
        vel.x = -speed
    elseif Input.IsKeyPressed(Key.D) then
        vel.x = speed
    else
        vel.x = 0
    end

    entity:SetLinearVelocity(Vec2(vel.x, vel.y))

    if is_grounded and Input.IsKeyPressed(Key.Space) then
        entity:SetLinearVelocity(Vec2(0, 0))
        entity:ApplyImpulse(Vec2(0, 0.2))
    end
end

function OnCollisionBegin(other)
    is_grounded = true
end

function OnCollisionEnd(other)
    is_grounded = false
end