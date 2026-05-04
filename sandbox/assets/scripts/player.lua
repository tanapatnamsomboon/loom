function OnUpdate(ts)
    if Input.IsKeyPressed(Key.Space) then
        entity:ApplyImpulse(Vec2(0, 0.1))
    end
    local vel = entity:GetLinearVelocity()
    Log.Info("vel: " .. tostring(vel.x) .. ", " .. tostring(vel.y))
end