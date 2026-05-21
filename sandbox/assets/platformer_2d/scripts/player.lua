-- player.lua — 2D platformer controller for the cat.
--
-- Attach to the Player entity. Requires, on the same entity:
--   * Rigidbody2D (Dynamic, FixedRotation = true)
--   * BoxCollider2D
--   * AnimationComponent with clips named exactly "idle", "run", "jump"
--   * AudioSource (jump SFX)
--
-- Tunables — edit and save; the engine hot-reloads the script.
local MoveSpeed       = 5.0   -- horizontal speed (world units / second)
local JumpSpeed       = 9.0   -- upward velocity applied on jump (units / second)
local PlayerHalfHeight= 0.72  -- collider half-extent Y in WORLD units (= BoxCollider2D.Size.y * Transform.Scale.y)
local SpriteFacesRight= false -- cat art faces left by default

local jumpWasDown = false     -- edge-detect Space so holding it doesn't auto-jump

-- Ground check: a short ray straight down from the player's center. A ray that
-- starts inside the player's own collider is ignored by Box2D, so this only
-- reports the surface below. Coin sensors are filtered out explicitly.
local function IsGrounded()
    local p = entity:GetTranslation()
    local r = Physics.Raycast(Vec3(p.x, p.y, 0), Vec3(0, -1, 0), PlayerHalfHeight + 0.08)
    if not r.hit then return false end
    if r.entity and r.entity:GetTag() == "Coin" then return false end
    return true
end

function OnCreate()
    entity:PlayAnimation("idle")
end

function OnUpdate(ts)
    local vel = entity:GetLinearVelocity()

    -- Horizontal input
    local vx = 0
    if Input.IsKeyPressed(Key.A) then vx = vx - MoveSpeed end
    if Input.IsKeyPressed(Key.D) then vx = vx + MoveSpeed end
    local vy = vel.y

    -- Jump on the rising edge of Space, only while grounded
    local grounded = IsGrounded()
    local jumpDown = Input.IsKeyPressed(Key.Space)
    if grounded and jumpDown and not jumpWasDown then
        vy = JumpSpeed
        entity:PlayAudio()
        grounded = false
    end
    jumpWasDown = jumpDown

    entity:SetLinearVelocity(Vec2(vx, vy))

    -- Face the movement direction by flipping the sprite's X scale sign
    if vx ~= 0 then
        local want = vx > 0 and 1 or -1
        if not SpriteFacesRight then want = -want end
        local s = entity:GetScale()
        entity:SetScale(Vec3(math.abs(s.x) * want, s.y, s.z))
    end

    -- Animation state
    if not grounded then
        -- 3-frame jump clip (0 = launch, 1 = peak, 2 = fall) driven by the
        -- actual vertical velocity, so the pose tracks the jump arc instead
        -- of running on a fixed timer.
        entity:PlayAnimation("jump")
        local frame = 1
        if vy > 1.5 then frame = 0
        elseif vy < -1.5 then frame = 2 end
        entity:SetAnimationFrame(frame)
    elseif vx ~= 0 then
        entity:PlayAnimation("run")
    else
        entity:PlayAnimation("idle")
    end
end
