local timer = 0

function OnUpdate(ts)
    timer = timer + ts
    if timer < 1.0 then return end
    timer = 0

    local pos = entity:GetTranslation()
    local center = Vec2(pos.x, pos.y)

    local circle_hits = Physics.OverlapCircle(center, 3.0)
    --Log.Info("OverlapCircle found: " .. #circle_hits .. " entities")
    --for i, e in ipairs(circle_hits) do
    --    Log.Info("  [" .. i .. "] " .. e:GetTag())
    --end

    local box_hits = Physics.OverlapBox(center, Vec2(0.5, 0.5))
    --Log.Info("OverlapBox found: " .. #box_hits .. " entities")
    --for i, e in ipairs(box_hits) do
    --    Log.Info("  [" .. i .. "] " .. e:GetTag())
    --end
end