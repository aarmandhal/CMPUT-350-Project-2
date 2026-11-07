#pragma once

#include <unordered_map>

#include "MathUtil.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <vector>
#include <string>

namespace ECSEngine {

using SpriteID = size_t;

class SpriteManager {
public:
    SpriteManager() = default;
    ~SpriteManager() = default;
    [[nodiscard]] SpriteID RegisterTexture(const std::string& texturePath, const Rect& sourceRect);
    sf::Sprite& GetSprite(SpriteID id);

private:

    // store textures using their file paths
    std::unordered_map<std::string, sf::Texture> mTextures;

    // store sprites that reference the textures
    std::vector<sf::Sprite> mSprites;

};

}  // namespace ECSEngine
