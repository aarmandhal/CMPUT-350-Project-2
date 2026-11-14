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

    // loads a texture if it has not already been loaded
    // creates a sprite and returns its id for later access
    // the manager owns the texture and sprite
    // throws std::runtime_error if the texture file fails to load
    [[nodiscard]] SpriteID RegisterTexture(const std::string& texturePath, const Rect& sourceRect);

    // returns a reference to a sprite stored in this manager
    // the reference is valid until the sprite vector reallocates
    // do not keep the reference when registering more sprites
    // the reference also becomes invalid when the manager is destroyed
    // throws std::out_of_range if the id was not returned by RegisterTexture
    sf::Sprite& GetSprite(SpriteID id);

private:

    // store textures using their file paths
    std::unordered_map<std::string, sf::Texture> mTextures;

    // store sprites that reference the textures
    std::vector<sf::Sprite> mSprites;

};

}  // namespace ECSEngine
