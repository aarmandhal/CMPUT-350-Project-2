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

    // loads and stores a sound using the given name
    // the manager owns the sound buffer and playable sound
    // registering an existing name does nothing
    void RegisterSound(const std::string& soundPath, const std::string& soundName);

    // returns a reference to a sound stored in this manager
    // the reference is valid until the sound vector reallocates
    // do not keep the reference when registering more sounds
    // the reference also becomes invalid when the manager is destroyed
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
