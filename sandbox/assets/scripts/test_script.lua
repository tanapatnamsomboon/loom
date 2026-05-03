function OnCreate()
    Log.Info("=== ScriptingEngine OK: OnCreate fired ===")
    Log.Info("Entity tag: " .. entity.GetTag())
end

function OnUpdate(ts)
    Log.Trace("OnUpdate ts=" .. ts)
end

function OnDestroy()
    Log.Info("=== OnDestroy fired ===")
end