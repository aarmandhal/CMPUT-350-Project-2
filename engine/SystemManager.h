#pragma once

#include <vector>
// #include "ECSEngine.h"

namespace ECSEngine {

// what a system returns each frame: kContinue keeps the game running, kExit stops it
enum tSystemReturn { kContinue, kExit };

template <typename... Components>
class EngineView;

// base class for every system
// a system gets the engine view each frame and uses it to read / change entities
template <typename... Components>
class System {
public:
    virtual ~System() = default;

    // called once per frame; return kExit to stop the game loop
    virtual tSystemReturn Run(EngineView<Components...>* scene) = 0;

    // name used when printing which system asked to exit
    virtual const std::string& GetName() const = 0;
};

// holds every system and runs them once per frame
// - systems run in the order they were added with AddSystem
// - the manager owns the systems (unique_ptr), so they are destroyed with the manager
//   or when ClearSystems is called
template <typename... Components>
class SystemManager {
public:
    // runs every system in order
    // if one returns kExit, the remaining systems are skipped and false is returned,
    // which makes the engine stop the game loop; otherwise returns true
    bool Run(EngineView<Components...>* scene) {
        for (auto& system : mSystems) {
            if (system->Run(scene) == kExit)  // system wants to exit
            {
                std::cout << system->GetName() << " requested exit from system manager\n";
                return false;
            }
        }
        return true;
    }

    // adds a system to the end of the run order; the manager takes ownership of it
    void AddSystem(std::unique_ptr<System<Components...>>&& system) {
        mSystems.push_back(std::move(system));
    }

    // removes and destroys every system
    void ClearSystems() { mSystems.clear(); }

private:
    // systems in the order they run
    std::vector<std::unique_ptr<System<Components...>>> mSystems;
};

}  // namespace ECSEngine
