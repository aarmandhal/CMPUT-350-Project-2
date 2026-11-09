#pragma once

#include "Keyboard.h"

namespace ECSEngine {

const float kMaxRunVelocity = 15;
const float kDashVelocity = 50;
const float kJumpVelocity = 25;
const float kGroundRunAcceleration = 5;
const float kAirRunAcceleration = 4;
const float kDashInterval = 25;
const float kWallSlideMaxFallSpeed = 5;
const float kJumpReleaseDamping = 0.5f;

template <typename... Components>
class InputSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("InputSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();
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

            if (justPressed(Key::Z) && collision.onGround) {
                movement.velocity.y = -kJumpVelocity;
            } else if (!input.keydown[Key::Z] && movement.velocity.y < 0) {
                movement.velocity.y *= kJumpReleaseDamping;
            }

            if (justPressed(Key::C) && (mFrameCount - input.lastDash) >= kDashInterval) {
                movement.velocity.x = mFacing * kDashVelocity;
                input.lastDash = mFrameCount;
            }

            bool pushingIntoWall = (input.keydown[Key::Left] && collision.onLeftWall) ||
                                   (input.keydown[Key::Right] && collision.onRightWall);
            if (movement.velocity.y > 0 && pushingIntoWall) {
                bool justStarted = movement.velocity.y > kWallSlideMaxFallSpeed;
                movement.velocity.y = std::min(movement.velocity.y, kWallSlideMaxFallSpeed);
                if (justStarted) engine->GetSoundManager().GetSound("wall").play();
            }
        }
        return kContinue;
    }

private:
    int mFrameCount = 0;
    int mFacing = 1;
};

}  // namespace ECSEngine