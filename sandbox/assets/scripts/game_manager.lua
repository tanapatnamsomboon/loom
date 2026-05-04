function OnCreate()
    entity:SetPitch(1.5)
    entity:PlayAudio()
    Log.Info("Playing: " .. tostring(entity:IsAudioPlaying()))
end

function OnUpdate(ts)
    if Input.IsKeyPressed(Key.S) then entity:StopAudio() end
    if Input.IsKeyPressed(Key.P) then entity:PlayAudio() end
end