#include "SoundManager.h"

#include <iostream>

namespace ECSEngine {

void SoundManager::RegisterSound(const std::string& soundPath, const std::string& soundName) {

    // dont register the same sound twice
    if (mSounds.find(soundName) != mSounds.end()) {
        return;
    }

    // get the buffer for this sound
    sf::SoundBuffer& buffer = mSoundBuffers[soundName];

    // load audio file
    if (!buffer.loadFromFile(soundPath)) {
        std::cerr << "Failed to load sound: " << soundPath << "\n";
        mSoundBuffers.erase(soundName);
        return;
    }

    // create a sound using the stored buffer
    mSoundInstances.emplace_back(buffer);

    // save the index of the sound
    mSounds[soundName] = mSoundInstances.size() - 1;
}

sf::Sound& SoundManager::GetSound(const std::string& soundName) {

    // find the index of the requested sound
    size_t index = mSounds.at(soundName);

    // return the sound
    return mSoundInstances[index];
}

}  // namespace ECSEngine