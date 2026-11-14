#pragma once

#include <algorithm>

#include "Keyboard.h"
#include "MathUtil.h"

namespace ECSEngine {

// all speeds are in pixels per frame, all times are in frames (the game runs at 60 fps)

// running
const float kMaxRunVelocity = 15;          // top horizontal running speed
const float kGroundRunAcceleration = 1.5f; // speed gained per frame on the ground (~10 frames to top speed)
const float kAirRunAcceleration = 1.0f;    // speed gained per frame in the air (less air control)

// jumping / falling
const float kJumpVelocity = 25;            // upward speed at the start of a ground jump
const float kJumpReleaseDamping = 0.5f;    // upward speed is multiplied by this each frame Z is not held
const float kFastFallAcceleration = 0.5f;  // extra downward speed per frame while holding down
const float kMaxFallSpeed = 24;            // fall speed limit (keeps the player from skipping through tiles)
const float kMaxFastFallSpeed = 32;        // fall speed limit while holding down
const float kWallSlideMaxFallSpeed = 5;    // fall speed limit while sliding down a wall

// wall jump
const float kWallJumpVelocityX = 15;       // sideways speed away from the wall
const float kWallJumpVelocityY = 22;       // upward speed of a wall jump
const int kWallJumpLockFrames = 10;        // frames left/right input is ignored after a wall jump
const float kWallHopVelocityY = 16;        // upward speed of a same-wall hop
const int kMaxWallHops = 2;                // hops allowed per wall before touching ground / another wall
const int kWallGraceFrames = 6;            // frames after leaving a wall where Z still works

// dash
const float kDashVelocity = 50;            // horizontal speed during a dash
const int kDashFrames = 6;                 // length of a dash (6 * 50 = 300 px)
const int kDashInterval = 60;              // frames between dashes (1 second cooldown)

// turns keyboard input into velocities for every entity that has
// an InputComponent, a MovementComponent and a CollisionComponent
// uses the collision flags from the previous frame to decide what the player can do
template <typename... Components>
class InputSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("InputSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        // managers needed for input
        auto& em = engine->GetEntityManager();
        auto& sounds = engine->GetSoundManager();

        // count frames so the dash cooldown can be measured
        mFrameCount++;

        // go through all valid entities
        for (auto& entity : em) {
            EntityID id = entity.GetID();

            // only process entities controlled by the player
            if (!em.template HasComponent<InputComponent>(id) ||
                !em.template HasComponent<MovementComponent>(id) ||
                !em.template HasComponent<CollisionComponent>(id))
                continue;

            // get the components needed to move the player
            auto& input = em.template GetComponent<InputComponent>(id);
            auto& movement = em.template GetComponent<MovementComponent>(id);
            auto& collision = em.template GetComponent<CollisionComponent>(id);

            // true only on the first frame a key goes down (down now, but was up last frame)
            // used so holding a key does not repeat jumps or dashes every frame
            auto justPressed = [&](unsigned key) {
                return input.keydown[key] && !input.lastKeydown[key];
            };

            // in progress dash
            // a dash overrides all other input until it finishes
            if (input.dashFramesLeft > 0) {
                // check if the dash ran into a wall in the direction we are dashing
                // facing right (> 0) -> only a wall on the right stops the dash
                // facing left (< 0) -> only a wall on the left stops the dash
                bool hitWall =
                    (mFacing > 0 && collision.onRightWall) || (mFacing < 0 && collision.onLeftWall);

                // hit a wall -> end the dash now (0 frames left)
                // no wall -> count down one frame
                input.dashFramesLeft = hitWall ? 0 : input.dashFramesLeft - 1;

                // still dashing: constant horizontal speed with no vertical movement,
                // then skip the rest of the input handling for this frame
                if (input.dashFramesLeft > 0) {
                    movement.velocity = Point2D(mFacing * kDashVelocity, 0.f);
                    continue;
                }

                // the dash just ended this frame
                // stopped by a wall -> no horizontal speed
                // not stopped -> drop back to normal top run speed in the dash direction
                movement.velocity.x = hitWall ? 0.f : mFacing * kMaxRunVelocity;

                // turn gravity back on (it was turned off when the dash started)
                SetGravityTimer(em, id, 0);
            }

            // horizontal running
            // right after a wall jump, left/right input is ignored for a few frames
            if (input.wallJumpLock > 0) {
                input.wallJumpLock--;
            } else {
                // on the ground -> stronger acceleration
                // in the air -> weaker acceleration (less control mid-jump)
                float accel = collision.onGround ? kGroundRunAcceleration : kAirRunAcceleration;

                if (input.keydown[Key::Right]) {
                    // holding right: speed up to the right, never past the top speed
                    movement.velocity.x = std::min(movement.velocity.x + accel, kMaxRunVelocity);
                    mFacing = 1;
                } else if (input.keydown[Key::Left]) {
                    // holding left: speed up to the left, never past the top speed (negative)
                    movement.velocity.x = std::max(movement.velocity.x - accel, -kMaxRunVelocity);
                    mFacing = -1;
                } else if (movement.velocity.x > 0) {
                    // no key held while moving right: slow down toward 0 without passing it
                    movement.velocity.x = std::max(movement.velocity.x - accel, 0.f);
                } else if (movement.velocity.x < 0) {
                    // no key held while moving left: slow down toward 0 without passing it
                    movement.velocity.x = std::min(movement.velocity.x + accel, 0.f);
                }
            }

            // wall contact tracking
            // the collision flags are from the end of last frame (CollisionSystem)
            // a wall flag is only set if the player moved into the wall, so this means
            // "pushing into a wall while in the air"
            bool touchingWall =
                !collision.onGround && (collision.onLeftWall || collision.onRightWall);

            if (collision.onGround) {
                // on the ground: forget the last wall and refill the same-wall hops
                input.wallSide = 0;
                input.wallGraceFrames = 0;
                input.wallHopsLeft = kMaxWallHops;
            } else if (touchingWall) {
                // wall on our left -> -1, otherwise the wall is on our right -> +1
                int side = collision.onLeftWall ? -1 : 1;

                // touching a different wall than last time refills the hops
                // (coming back to the same wall does not)
                if (side != input.wallSide) input.wallHopsLeft = kMaxWallHops;

                // remember this wall and restart the grace window
                input.wallSide = side;
                input.wallGraceFrames = kWallGraceFrames;
            } else if (input.wallGraceFrames > 0) {
                // in the air and no longer touching a wall: count the grace window down
                input.wallGraceFrames--;
            }

            // a wall jump or hop is allowed while the grace window is open and we know the wall side
            bool canWallJump = input.wallGraceFrames > 0 && input.wallSide != 0;

            // is the arrow pointing away from the wall held?
            // wall on the right (+1) -> away is the left arrow
            // wall on the left (-1) -> away is the right arrow
            bool awayHeld =
                (input.wallSide > 0) ? input.keydown[Key::Left] : input.keydown[Key::Right];

            // jumping
            if (justPressed(Key::Z) && collision.onGround) {
                // ground jump: negative y is up
                movement.velocity.y = -kJumpVelocity;
                sounds.GetSound("jump").play();
            } else if (justPressed(Key::Z) && canWallJump && awayHeld) {
                // wall jump to the other side (Z + the arrow away from the wall)
                // -wallSide is the direction away from the wall
                // (wall on the right is +1, so -1 = jump left)
                movement.velocity.x = -input.wallSide * kWallJumpVelocityX;
                movement.velocity.y = -kWallJumpVelocityY;
                mFacing = -input.wallSide;

                // ignore left/right for a few frames so the player actually leaves the wall
                input.wallJumpLock = kWallJumpLockFrames;

                // close the grace window so a quick second Z can't wall jump off this wall again
                input.wallGraceFrames = 0;
                sounds.GetSound("jump").play();
            } else if (justPressed(Key::Z) && canWallJump) {
                // same-wall hop (Z while holding toward the wall or no arrow)
                // only if the player still has hops left for this wall
                if (input.wallHopsLeft > 0) {
                    input.wallHopsLeft--;
                    movement.velocity.y = -kWallHopVelocityY;

                    // wallSide points toward the wall, so this pushes 1 px/frame into it
                    // to keep the player against the wall (too small to cause a screen shake)
                    movement.velocity.x = input.wallSide * kAirRunAcceleration;
                    sounds.GetSound("jump").play();
                }
            } else if (!input.keydown[Key::Z] && movement.velocity.y < 0) {
                // jump key released while still moving up: halve the upward speed each frame
                // this is what makes a tap a short jump and holding a full jump
                movement.velocity.y *= kJumpReleaseDamping;
            }

            // falling
            // holding down while falling (positive y is down) adds extra downward speed
            if (input.keydown[Key::Down] && movement.velocity.y > 0) {
                movement.velocity.y += kFastFallAcceleration;
            }

            // limit the fall speed
            // holding down -> higher fast-fall limit, otherwise -> normal limit
            float maxFall = input.keydown[Key::Down] ? kMaxFastFallSpeed : kMaxFallSpeed;
            movement.velocity.y = std::min(movement.velocity.y, maxFall);

            // wall slide
            // pushing into a wall = holding left against a wall on the left,
            // or holding right against a wall on the right
            bool pushingIntoWall = (input.keydown[Key::Left] && collision.onLeftWall) ||
                                   (input.keydown[Key::Right] && collision.onRightWall);

            // falling while pushing into a wall slows the fall down to the slide speed
            if (movement.velocity.y > 0 && pushingIntoWall) {
                // falling faster than the slide speed means the slide is only starting now
                bool justStarted = movement.velocity.y > kWallSlideMaxFallSpeed;
                movement.velocity.y = std::min(movement.velocity.y, kWallSlideMaxFallSpeed);

                // play the wall sound once at the start of the slide, not every frame
                if (justStarted) sounds.GetSound("wall").play();
            }

            // start dash
            // only on the frame C is pressed, and only if the cooldown has passed
            if (justPressed(Key::C) && (mFrameCount - input.lastDash) >= kDashInterval) {
                // remember when this dash started for the cooldown
                input.lastDash = mFrameCount;
                input.dashFramesLeft = kDashFrames;

                // dash straight in the facing direction with no vertical movement
                movement.velocity = Point2D(mFacing * kDashVelocity, 0.f);

                // turn off gravity for the dash so it travels in a straight line
                SetGravityTimer(em, id, kDashFrames);
            }
        }
        return kContinue;
    }

private:
    // sets how many frames gravity is turned off for an entity (0 turns it back on)
    // GravitySystem skips entities while their disabledTimer is above 0
    template <typename EM>
    static void SetGravityTimer(EM& em, EntityID id, int frames) {
        if (em.template HasComponent<GravityComponent>(id))
            em.template GetComponent<GravityComponent>(id).disabledTimer = frames;
    }

    // frames since the game started, used for the dash cooldown
    int mFrameCount = 0;

    // direction the player is facing: 1 = right, -1 = left (used for dash direction)
    // stored on the system, which works because there is only one player
    int mFacing = 1;
};

}  // namespace ECSEngine