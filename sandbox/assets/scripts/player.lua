local is_grounded = false;

function OnUpdate(ts)
    if is_grounded and Input.IsKeyPressed(Key.Space) then
        entity:SetLinearVelocity(Vec2(0, 0))
        entity:ApplyImpulse(Vec2(0, 0.2))
    end
end

function OnCollisionBegin(other)
    is_grounded = true;
end

function OnCollisionEnd(other)
    is_grounded = false;
end