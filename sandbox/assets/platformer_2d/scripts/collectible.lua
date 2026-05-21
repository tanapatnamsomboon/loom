-- collectible.lua — coin sensor pickup.
--
-- Attach to each Coin entity. Requires, on the same entity:
--   * Rigidbody2D (Static)
--   * BoxCollider2D with IsSensor = true
--
-- When the Player enters the coin's sensor it bumps the Score HUD, triggers
-- the shared SFXPlayer (the coin can't play its own SFX — it destroys itself),
-- then removes the coin.
--
-- Scene dependencies (resolved by Tag):
--   * an entity tagged "Score"     with a TextComponent reading "Score: N"
--   * an entity tagged "SFXPlayer" with an AudioSource (pickup SFX)

function OnSensorBegin(other)
    if other:GetTag() ~= "Player" then return end

    -- Bump the score counter parsed out of the HUD text.
    local hud = entity:FindByTag("Score")
    local current = tonumber(string.match(hud:GetText(), "%d+")) or 0
    hud:SetText("Score: " .. (current + 1))

    -- Trigger the shared one-shot SFX player.
    entity:FindByTag("SFXPlayer"):PlayAudio()

    entity:Destroy()
end
