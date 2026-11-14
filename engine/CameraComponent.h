#pragma once

#include <unordered_map>

#include "Entity.h"
#include "LocationComponent.h"
#include "MathUtil.h"
#include "SpriteComponent.h"
#include "SystemManager.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include "CameraFollowerComponent.h"
#include "CameraShakeComponent.h"

namespace ECSEngine {

struct CameraComponent {
    Point2D position;  // [world coordinates] position in world coordinates
    float worldUnitsPerPixel;
    Rect hardExclusion;  // [screen coordinates] Area where the camera is not allowed to go
                         // (negative of this rect)
    Rect softExclusion;  // [screen coordinates] Area where the camera is allowed to go (but will
                         // scroll out)
    float speed;
};

}  // namespace ECSEngine