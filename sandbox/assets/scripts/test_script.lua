function OnCreate()
    Log.Info("=== ScriptingEngine OK: OnCreate fired ===")
    Log.Info("Entity tag: " .. entity:GetTag())

    -- 1. Multi-arg Log
    Log.Info("pos:", entity:GetTranslation().x, entity:GetTranslation().y)

    -- 2. FindByTag
    local cam = entity:FindByTag("Camera")
    if cam then
        Log.Info("Found:", cam:GetTag())
    end

    -- 3. Spawn + Destroy
    local tmp = entity:Spawn()
    Log.Info("Spawned:", tmp:GetTag())
    tmp:Destroy()

    -- 4. Instantiate prefab (requires a saved .lprefab)
    local p = entity:Instantiate("prefabs/sun.lprefab")
    Log.Info("Prefab:", p:GetTag())
end

function OnUpdate(ts)

end

function OnDestroy()
    Log.Info("=== OnDestroy fired ===")
end