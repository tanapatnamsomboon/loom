function OnSensorBegin(other)
    Log.Info("Entered by: " .. other:GetTag())
end

function OnSensorEnd(other)
    Log.Info("Exited by: " .. other:GetTag())
end