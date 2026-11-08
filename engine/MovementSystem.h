#pragma once

namespace ECSEngine {

template <typename... Components>
class MovementSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("MovementSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();
        for (auto& entity : em) {
            EntityID id = entity.GetID();
            if (em.template HasComponent<MovementComponent>(id) &&
                em.template HasComponent<LocationComponent>(id)) {
                auto& m = em.template GetComponent<MovementComponent>(id);
                auto& l = em.template GetComponent<LocationComponent>(id);
                l.loc.x += m.velocity.x;
                l.loc.y += m.velocity.y;
            }
        }
        return kContinue;
    }
};

}  // namespace ECSEngine
