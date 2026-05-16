-- Animation events test: logs every OnAnimationEvent(name) fired by the active clip.
-- Pair with anim_test.lua (1/2 to switch clips) to verify events fire on the right frames,
-- on loop wrap-around, and on initial Play.

function OnAnimationEvent(name)
    Log.Info("[anim event] " .. name ..
             "  (clip=" .. entity:GetCurrentAnimation() ..
             ", frame=" .. entity:GetAnimationFrame() .. ")")
end
