#pragma once

#include "Entity.h"
namespace ECSEngine {

template <typename... Components>
class CollisionLateSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("CollisionLateSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();
        for (auto& entity : em) {
            EntityID id = entity.GetID();
            if (em.template HasComponent<CollisionComponent>(id)) {
                auto& c = em.template GetComponent<CollisionComponent>(id);
                c.previousCollisionBox = c.collisionBox;
            }
        }
        return kContinue;
    }
};

}  // namespace ECSEngine
