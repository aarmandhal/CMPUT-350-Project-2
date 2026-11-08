#pragma once

#include <unordered_map>

#include "LocationComponent.h"
#include "MathUtil.h"
#include "SpriteComponent.h"
#include "SystemManager.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "CameraComponent.h"
#include "EntityManager.h"
#include "LocationComponent.h"
#include "SystemManager.h"
#include "WindowManager.h"

namespace ECSEngine {

template <typename... Components>
class CameraSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name = "CameraSystem";
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override { 


        // managers needed for the camera
        auto& entities = engine->GetEntityManager();
        auto& windowManager = engine->GetWindowManager();

        // go through all valid entities
        for (const auto& entity : entities) {
            EntityID id = entity.GetID();

            // only process entities with a camera and follower
            if (!entities.template HasComponent<CameraComponent>(id) ||
                !entities.template HasComponent<CameraFollower>(id)) {
                continue;
            }

            // get the camera and the entity it follows
            auto& camera = entities.template GetComponent<CameraComponent>(id);
            auto& follower = entities.template GetComponent<CameraFollower>(id);

            // make sure the followed entity has a location
            if (!entities.template HasComponent<LocationComponent>(follower.entityID)) {
                continue;
            }

            auto& location = entities.template GetComponent<LocationComponent>(follower.entityID);

            // set camera scale and current position
            windowManager.SetWorldScale(camera.worldUnitsPerPixel);
            windowManager.SetWorldCenter(camera.position);

            // convert world coordinates to screen coordinates
            // find the player's position on the screen
            Point2D playerScreen = windowManager.WorldToWindow(location.loc);

            // calculate boundaries of the soft region
            float softLeft = camera.softExclusion.topLeft.x;
            float softRight = softLeft + camera.softExclusion.width;

            // calculate boundaries of the hard region
            float hardLeft = camera.hardExclusion.topLeft.x;
            float hardRight = hardLeft + camera.hardExclusion.width;

            // move immediately if player enters the hard region
            if (playerScreen.x < hardLeft) {
                camera.position.x += (playerScreen.x - hardLeft) * camera.worldUnitsPerPixel;
            }
            
            else if (playerScreen.x > hardRight) {
                camera.position.x += (playerScreen.x - hardRight) * camera.worldUnitsPerPixel;
            }

            // move slowly if the player is outside the dead zone
            else if (playerScreen.x < softLeft) {
                camera.position.x += (playerScreen.x - softLeft) * camera.worldUnitsPerPixel * camera.speed;
            }

            else if (playerScreen.x > softRight) {
                camera.position.x += (playerScreen.x - softRight) * camera.worldUnitsPerPixel * camera.speed;
            }

            // start with the updated camera position
            Point2D finalPosition = camera.position;

            // apply camera shake if the camera has an active shake
            if (entities.template HasComponent<CameraShake>(id)) {
                auto& shake = entities.template GetComponent<CameraShake>(id);

                if (shake.frameLimit > 0) {

                    // apply shake offset
                    finalPosition.x += shake.offset.x;
                    finalPosition.y += shake.offset.y;

                    // reduce shake over time
                    shake.offset.x *= 0.8f;
                    shake.offset.y *= 0.8f;
                    shake.frameLimit--;
                }
            }

            windowManager.SetWorldCenter(finalPosition);
        }

        return kContinue; 
    
    }
};

}  // namespace ECSEngine