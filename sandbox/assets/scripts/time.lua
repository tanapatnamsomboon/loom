local time = 0

function OnUpdate(ts)
    time = time + ts
    entity:SetText("Time: " .. string.format("%1.f",  time))
end