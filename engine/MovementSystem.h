#pragma once

namespace ECSEngine {

// moves every entity by its velocity once per frame
// runs after InputSystem and GravitySystem have set the velocity,
// and before CollisionSystem fixes any overlaps this movement caused
template <typename... Components>
class MovementSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("MovementSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();

        // go through all valid entities
        for (auto& entity : em) {
            EntityID id = entity.GetID();

            // only entities that have both a velocity and a position
            if (em.template HasComponent<MovementComponent>(id) &&
                em.template HasComponent<LocationComponent>(id)) {
                auto& m = em.template GetComponent<MovementComponent>(id);
                auto& l = em.template GetComponent<LocationComponent>(id);

                // new position = old position + velocity (pixels per frame)
                l.loc.x += m.velocity.x;
                l.loc.y += m.velocity.y;
            }
        }
        return kContinue;
    }
};

}  // namespace ECSEngine
