#pragma once

#include "CameraComponent.h"
#include "EntityManager.h"
#include "MathUtil.h"
#include "SpriteManager.h"
#include "SystemManager.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "SpriteComponent.h"
#include "LocationComponent.h"
#include "WindowManager.h"

namespace ECSEngine {

template <typename... Components>
class SpriteSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("SpriteSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override { 

        auto& entities = engine->GetEntityManager();
        auto& sprites = engine->GetSpriteManager();
        auto& windowManager = engine->GetWindowManager();
        auto window = windowManager.GetWindow();

        // clear previous frame
        window->clear(sf::Color::Black);

        // go through all valid entities
        for (const auto& entity : entities) {
            EntityID id = entity.GetID();

            // only draw entities that have a sprite and location
            if (!entities.template HasComponent<SpriteComponent>(id) ||
                !entities.template HasComponent<LocationComponent>(id)) {
                continue;
            }

            auto& spriteComponent = entities.template GetComponent<SpriteComponent>(id);
            auto& location = entities.template GetComponent<LocationComponent>(id);
            auto& sprite = sprites.GetSprite(spriteComponent.textureID);

            // calculate drawing rectangle relative to the entity
            Rect bounds = spriteComponent.bounds;
            bounds.topLeft.x += location.loc.x;
            bounds.topLeft.y += location.loc.y;

            // convert world coordinates to screen coordinates if needed
            if (spriteComponent.worldSpace) {
                bounds = windowManager.WorldToWindow(bounds);
            }

            sprite.setPosition({bounds.topLeft.x, bounds.topLeft.y});
            auto size = sprite.getLocalBounds().size;

            if (size.x != 0 && size.y != 0) {
                sprite.setScale({bounds.width / size.x, bounds.height / size.y});
            }

            // draw sprite
            window->draw(sprite);
        }

        window->display();

        return kContinue; 
    }

};

}  // namespace ECSEngine