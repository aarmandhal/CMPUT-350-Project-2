#pragma once

#include "CollisionComponent.h"
#include "Entity.h"
#include "LocationComponent.h"
namespace ECSEngine {

// the spec's "CollisionSystemUpdate"
// runs FIRST each frame, before anything moves, and saves where every collision box is
// in the world. CollisionSystem later compares this saved box to the new position
// to work out which side an entity hit something from
template <typename... Components>
class CollisionLateSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("CollisionLateSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();

        // go through all valid entities
        for (auto& entity : em) {
            EntityID id = entity.GetID();

            // only entities that can collide and have a position
            if (!em.template HasComponent<CollisionComponent>(id) ||
                !em.template HasComponent<LocationComponent>(id))
                continue;

            auto& c = em.template GetComponent<CollisionComponent>(id);
            const auto& loc = em.template GetComponent<LocationComponent>(id).loc;

            // collisionBox is stored relative to the entity location,
            // so add the location to get the box in world coordinates
            c.previousCollisionBox = c.collisionBox;
            c.previousCollisionBox.topLeft.x += loc.x;
            c.previousCollisionBox.topLeft.y += loc.y;
        }
        return kContinue;
    }
};

}  // namespace ECSEngine
