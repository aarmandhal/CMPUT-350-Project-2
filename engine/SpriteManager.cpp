#include "SpriteManager.h"
#include <stdexcept>

namespace ECSEngine {

SpriteID SpriteManager::RegisterTexture(const std::string& texturePath, const Rect& sourceRect) {
    // check if the texture has already been loaded
    if (mTextures.find(texturePath) == mTextures.end()) {

        // load the texture from the file
        if (!mTextures[texturePath].loadFromFile(texturePath)) {
            mTextures.erase(texturePath);
            throw std::runtime_error("Failed to load texture: " + texturePath);
        }
    }

    // convert the source rectangle to sfml coordinates
    sf::IntRect rect(
        {static_cast<int>(sourceRect.topLeft.x), static_cast<int>(sourceRect.topLeft.y)},
        {static_cast<int>(sourceRect.width), static_cast<int>(sourceRect.height)}
    );

    // create a sprite using the stored texture and source rectangle
    mSprites.emplace_back(mTextures.at(texturePath), rect);

    // return the index of the new sprite
    return mSprites.size() - 1;

}

sf::Sprite& SpriteManager::GetSprite(SpriteID id) {
    return mSprites.at(id);
}

}  // namespace ECSEngine