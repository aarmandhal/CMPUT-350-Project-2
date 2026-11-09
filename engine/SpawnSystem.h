#pragma once

#include <cstdlib>

#include "SystemManager.h"
#include "EntityManager.h"
#include "LocationComponent.h"
#include "SpriteComponent.h"
#include "CollisionComponent.h"
#include "SpawnComponent.h"

namespace ECSEngine {

template <typename... Components>
class SpawnSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("SpawnSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override { 

        auto& entities = engine->GetEntityManager();

        // avoid creating entities while iterating over them
        std::vector<EntityID> readySpawners;

        for (const auto& entity : entities) {
            EntityID id = entity.GetID();

            // only entities with spawn settings and a position can spawn
            if (!entities.template HasComponent<SpawnComponent>(id) ||
                !entities.template HasComponent<LocationComponent>(id)) {
                continue;
            }

            auto& spawn = entities.template GetComponent<SpawnComponent>(id);

            // skip spawners that have reached their limit
            if (spawn.totalSpawns <= 0) {
                continue;
            }

            spawn.timeToNextSpawn--;

            // only spawn when the timer expires
            if (spawn.timeToNextSpawn > 0) {
                continue;
            }

            readySpawners.push_back(id);

            // reset the timer and track remaining spawns
            spawn.totalSpawns--;
            spawn.timeToNextSpawn = spawn.timeBetweenSpawns;
        }

        // create stars after the iteration to keep iterators valid
        for (EntityID id : readySpawners) {
            auto& spawn = entities.template GetComponent<SpawnComponent>(id);
            auto& location = entities.template GetComponent<LocationComponent>(id);

            // preserve spawner data before modifying entity storage
            auto spawnType = spawn.spawnType;
            auto textureID = spawn.spawnTextureID;
            auto spawnPosition = location.loc;

            EntityID starID = entities.CreateEntity(spawnType);

            LocationComponent starLocation{};
            starLocation.loc = {spawnPosition.x + 16.0f, spawnPosition.y + 16.0f};
            entities.template AddComponent<LocationComponent>(starID, starLocation);

            SpriteComponent starSprite{};
            starSprite.textureID = textureID;
            starSprite.bounds = {{0.0f, 0.0f}, 32.0f, 32.0f};
            starSprite.worldSpace = true;
            entities.template AddComponent<SpriteComponent>(starID, starSprite);

            // give spawned stars different movement directions
            MovementComponent movement{};
            movement.velocity = {static_cast<float>(std::rand() % 11 - 5), static_cast<float>(std::rand() % 11 - 5)};
            entities.template AddComponent<MovementComponent>(starID, movement);

            // use matching sprite and collision dimensions
            CollisionComponent collision{};
            collision.staticEntity = false;
            collision.doesDamage = false;
            collision.collisionBox = {{0.0f, 0.0f}, 32.0f, 32.0f};
            collision.previousCollisionBox = {starLocation.loc, 32.0f, 32.0f};
            entities.template AddComponent<CollisionComponent>(starID, collision);
        }    

        return kContinue; 
        
    }
};

}  // namespace ECSEngine
