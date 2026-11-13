#pragma once

namespace ECSEngine {

// how strongly gravity pulls depending on what the entity is doing
const float kFallGravityMultiplier = 1.6f;       // falling: stronger, so falls are faster than rises
const float kRiseGravityMultiplier = 1.0f;       // rising: normal gravity
const float kWallSlideGravityMultiplier = 0.3f;  // falling while pushing into a wall: much weaker

// adds the acceleration from the GravityComponent to the velocity of every entity
// that has both a gravity and a movement component
// the strength depends on the entity's collisions from last frame
template <typename... Components>
class GravitySystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("GravitySystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();

        // go through all valid entities
        for (auto& entity : em) {
            EntityID id = entity.GetID();

            // only entities that have gravity and can move
            if (!em.template HasComponent<GravityComponent>(id) ||
                !em.template HasComponent<MovementComponent>(id))
                continue;

            auto& gravity = em.template GetComponent<GravityComponent>(id);
            auto& movement = em.template GetComponent<MovementComponent>(id);

            // gravity is turned off for a number of frames (used during a dash)
            // count the timer down and skip gravity this frame
            if (gravity.disabledTimer > 0) {
                gravity.disabledTimer--;
                continue;
            }

            // pick the gravity strength
            // moving down or standing still (y >= 0, positive y is down) -> fall gravity
            // moving up (y < 0) -> rise gravity
            float mult =
                (movement.velocity.y >= 0) ? kFallGravityMultiplier : kRiseGravityMultiplier;

            // gravity depends on last frame's collisions
            if (em.template HasComponent<CollisionComponent>(id)) {
                const auto& c = em.template GetComponent<CollisionComponent>(id);

                // touching a wall in the air (wall flags are only set when moving into the wall)
                bool onWall = !c.onGround && (c.onLeftWall || c.onRightWall);

                // falling while pushing into a wall (wall slide) -> weak gravity
                if (movement.velocity.y >= 0 && onWall) {
                    mult = kWallSlideGravityMultiplier;
                }
            }

            // apply the chosen gravity strength to the velocity
            // gravity is NOT turned off on the ground: pulling into the floor each frame is
            // what lets CollisionSystem detect that the entity is standing on something
            movement.velocity.x += mult * gravity.acceleration.x;
            movement.velocity.y += mult * gravity.acceleration.y;
        }
        return kContinue;
    }
};

}  // namespace ECSEngine
