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
    -- 5. Physics.Raycast (only meaningful during Play with Rigidbody2D in scene)
    local origin = Vec3(0, 5, 0)
    local dir    = Vec3(0, -1, 0)
    local hit    = Physics.Raycast(origin, dir, 20.0)
    if hit.hit then
        Log.Info("Hit:", hit.entity:GetTag(), "at y=", hit.point.y)
    end
end

function OnDestroy()
    Log.Info("=== OnDestroy fired ===")
end