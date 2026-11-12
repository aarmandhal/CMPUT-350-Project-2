#pragma once

#include <algorithm>

#include "Keyboard.h"
#include "MathUtil.h"

namespace ECSEngine {

// Running
const float kMaxRunVelocity = 15;
const float kGroundRunAcceleration = 1.5f;
const float kAirRunAcceleration = 1.0f;

// Jumping / falling
const float kJumpVelocity = 25;
const float kJumpReleaseDamping = 0.5f;
const float kFastFallAcceleration = 0.5f;
const float kMaxFallSpeed = 24;
const float kMaxFastFallSpeed = 32;
const float kWallSlideMaxFallSpeed = 5;

// Wall jump
const float kWallJumpVelocityX = 15;
const float kWallJumpVelocityY = 22;
const int kWallJumpLockFrames = 10;

// Dash
const float kDashVelocity = 50;
const int kDashFrames = 6;
const int kDashInterval = 60;

template <typename... Components>
class InputSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("InputSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();
        auto& sounds = engine->GetSoundManager();
        mFrameCount++;

        for (auto& entity : em) {
            EntityID id = entity.GetID();
            if (!em.template HasComponent<InputComponent>(id) ||
                !em.template HasComponent<MovementComponent>(id) ||
                !em.template HasComponent<CollisionComponent>(id))
                continue;

            auto& input = em.template GetComponent<InputComponent>(id);
            auto& movement = em.template GetComponent<MovementComponent>(id);
            auto& collision = em.template GetComponent<CollisionComponent>(id);

            auto justPressed = [&](unsigned key) {
                return input.keydown[key] && !input.lastKeydown[key];
            };

            // In Progress Dash
            if (input.dashFramesLeft > 0) {
                bool hitWall =
                    (mFacing > 0 && collision.onRightWall) || (mFacing < 0 && collision.onLeftWall);
                input.dashFramesLeft = hitWall ? 0 : input.dashFramesLeft - 1;
                if (input.dashFramesLeft > 0) {
                    movement.velocity = Point2D(mFacing * kDashVelocity, 0.f);
                    continue;
                }
                movement.velocity.x = hitWall ? 0.f : mFacing * kMaxRunVelocity;
                SetGravityTimer(em, id, 0);
            }

            // Horizontal Running
            if (input.wallJumpLock > 0) {
                input.wallJumpLock--;
            } else {
                float accel = collision.onGround ? kGroundRunAcceleration : kAirRunAcceleration;
                if (input.keydown[Key::Right]) {
                    movement.velocity.x = std::min(movement.velocity.x + accel, kMaxRunVelocity);
                    mFacing = 1;
                } else if (input.keydown[Key::Left]) {
                    movement.velocity.x = std::max(movement.velocity.x - accel, -kMaxRunVelocity);
                    mFacing = -1;
                } else if (movement.velocity.x > 0) {
                    movement.velocity.x = std::max(movement.velocity.x - accel, 0.f);
                } else if (movement.velocity.x < 0) {
                    movement.velocity.x = std::min(movement.velocity.x + accel, 0.f);
                }
            }

            // Jumping
            bool onWall = !collision.onGround && (collision.onLeftWall || collision.onRightWall);
            if (justPressed(Key::Z) && collision.onGround) {
                movement.velocity.y = -kJumpVelocity;
                sounds.GetSound("jump").play();
            } else if (justPressed(Key::Z) && onWall) {
                float away = collision.onLeftWall ? 1.f : -1.f;
                movement.velocity.x = away * kWallJumpVelocityX;
                movement.velocity.y = -kWallJumpVelocityY;
                mFacing = collision.onLeftWall ? 1 : -1;
                input.wallJumpLock = kWallJumpLockFrames;
                sounds.GetSound("jump").play();
            } else if (!input.keydown[Key::Z] && movement.velocity.y < 0) {
                movement.velocity.y *= kJumpReleaseDamping;
            }

            // Falling
            if (input.keydown[Key::Down] && movement.velocity.y > 0) {
                movement.velocity.y += kFastFallAcceleration;
            }
            float maxFall = input.keydown[Key::Down] ? kMaxFastFallSpeed : kMaxFallSpeed;
            movement.velocity.y = std::min(movement.velocity.y, maxFall);

            bool pushingIntoWall = (input.keydown[Key::Left] && collision.onLeftWall) ||
                                   (input.keydown[Key::Right] && collision.onRightWall);
            if (movement.velocity.y > 0 && pushingIntoWall) {
                bool justStarted = movement.velocity.y > kWallSlideMaxFallSpeed;
                movement.velocity.y = std::min(movement.velocity.y, kWallSlideMaxFallSpeed);
                if (justStarted) sounds.GetSound("wall").play();
            }

            // Start Dash
            if (justPressed(Key::C) && (mFrameCount - input.lastDash) >= kDashInterval) {
                input.lastDash = mFrameCount;
                input.dashFramesLeft = kDashFrames;
                movement.velocity = Point2D(mFacing * kDashVelocity, 0.f);
                SetGravityTimer(em, id, kDashFrames);
            }
        }
        return kContinue;
    }

private:
    template <typename EM>
    static void SetGravityTimer(EM& em, EntityID id, int frames) {
        if (em.template HasComponent<GravityComponent>(id))
            em.template GetComponent<GravityComponent>(id).disabledTimer = frames;
    }

    int mFrameCount = 0;
    int mFacing = 1;
};

}  // namespace ECSEngine