local elapsed = 0

function OnCreate()
    Log.Info("Scene A started!")
end

function OnUpdate(ts)
    elapsed = elapsed + ts
    if elapsed > 3.0 then
        Log.Info("Transitioning to scene_b.loom...")
        Scene.Load("scenes/scene_b.loom")
    end
end