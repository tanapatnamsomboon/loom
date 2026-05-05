local is_grounded = false

Properties = {
    Speed  = 5.0,
    Health = 100,
    Active = true,
    Label  = "Hero"
}

function OnCreate()
    Log.Info("Speed =", Speed, " Health =", Health, " Label =", Label)
end

function OnUpdate(ts)
    local vel = entity:GetLinearVelocity()

    if Input.IsKeyPressed(Key.A) then
        vel.x = -Speed
    elseif Input.IsKeyPressed(Key.D) then
        vel.x = Speed
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