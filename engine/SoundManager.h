#pragma once

#include <unordered_map>

#include <SFML/Audio.hpp>

#include <vector>
#include <string>

namespace ECSEngine {

class SoundManager {
public:
    SoundManager() = default;
    ~SoundManager() = default;
    void RegisterSound(const std::string& soundPath, const std::string& soundName);
    sf::Sound& GetSound(const std::string& soundName);

private:
    // store the audio data for each sound
    std::unordered_map<std::string, sf::SoundBuffer> mSoundBuffers;
    
    // map each sound name to its index
    std::unordered_map<std::string, size_t> mSounds;

    // store the playable sounds
    std::vector<sf::Sound> mSoundInstances;
};

}  // namespace ECSEngine
