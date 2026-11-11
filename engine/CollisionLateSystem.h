#pragma once

#include "CollisionComponent.h"
#include "Entity.h"
#include "LocationComponent.h"
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
            if (!em.template HasComponent<CollisionComponent>(id) ||
                !em.template HasComponent<LocationComponent>(id))
                continue;
            auto& c = em.template GetComponent<CollisionComponent>(id);
            const auto& loc = em.template GetComponent<LocationComponent>(id).loc;
            c.previousCollisionBox = c.collisionBox;
            c.previousCollisionBox.topLeft.x += loc.x;
            c.previousCollisionBox.topLeft.y += loc.y;
        }
        return kContinue;
    }
};

}  // namespace ECSEngine
