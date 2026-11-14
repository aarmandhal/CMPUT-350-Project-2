#pragma once

#include "SystemManager.h"
#include "EntityManager.h"
#include "ScoreComponent.h"
#include "SpriteComponent.h"

namespace ECSEngine {

template <typename... Components>
class ScoreSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("ScoreSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override { 

        auto& entities = engine->GetEntityManager();

        // go through all valid entities
        for (const auto& entity : entities) {
            EntityID id = entity.GetID();

            // only update entities that have a score component
            if (!entities.template HasComponent<ScoreComponent>(id)) {
                continue;
            }

            auto& scoreComponent = entities.template GetComponent<ScoreComponent>(id);
            int score = scoreComponent.score;

            // update each of the three score digits
            for (int i = 2; i >= 0; i--) {

                // get the last digit
                int digit = score % 10;

                // get the entity displaying this digit
                EntityID digitID = scoreComponent.digitID[i];

                // update the sprite for this digit
                if (entities.template HasComponent<SpriteComponent>(digitID)) {
                    auto& sprite = entities.template GetComponent<SpriteComponent>(digitID);

                    sprite.textureID = scoreComponent.digitTextureID[digit];
                }

                // move to next digit
                score /= 10;
            }
        }

        return kContinue; 

    }
};

}  // namespace ECSEngine
