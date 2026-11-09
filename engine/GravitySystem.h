#pragma once

namespace ECSEngine {

const float kFallGravityMultiplier = 1.6f;
const float kRiseGravityMultiplier = 1.0f;

template <typename... Components>
class GravitySystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("GravitySystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();
        for (auto& entity : em) {
            EntityID id = entity.GetID();
            if (!em.template HasComponent<GravityComponent>(id) ||
                !em.template HasComponent<MovementComponent>(id))
                continue;

            auto& gravity = em.template GetComponent<GravityComponent>(id);
            auto& movement = em.template GetComponent<MovementComponent>(id);

            if (gravity.disabledTimer > 0) {
                gravity.disabledTimer--;
                continue;
            }

            bool grounded = em.template HasComponent<CollisionComponent>(id) &&
                            em.template GetComponent<CollisionComponent>(id).onGround;
            if (grounded && movement.velocity.y >= 0) {
                movement.velocity.y = 0;
                continue;
            }

            float mult =
                (movement.velocity.y >= 0) ? kFallGravityMultiplier : kRiseGravityMultiplier;
            movement.velocity.x += mult * gravity.acceleration.x;
            movement.velocity.y += mult * gravity.acceleration.y;
        }
        return kContinue;
    }
};

}  // namespace ECSEngine
