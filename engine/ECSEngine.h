#pragma once

#include <optional>
#include <vector>

#include "CameraComponent.h"
#include "CollisionComponent.h"
#include "EntityManager.h"
#include "InputComponent.h"
#include "LocationComponent.h"
#include "MathUtil.h"
#include "ScoreComponent.h"
#include "SoundManager.h"
#include "SpawnComponent.h"
#include "SpriteComponent.h"
#include "SpriteManager.h"
#include "SystemManager.h"
#include "WindowManager.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

namespace ECSEngine {

// what systems are given each frame: access to the four resource managers
// systems use these instead of holding the managers themselves
template <typename... Components>
class EngineView {
public:
    virtual SoundManager& GetSoundManager() = 0;
    virtual SpriteManager& GetSpriteManager() = 0;
    virtual EntityManager<Components...>& GetEntityManager() = 0;
    virtual WindowManager& GetWindowManager() = 0;
};

// the engine owns every manager and runs the main game loop
template <typename... Components>
class ECSEngine : public EngineView<Components...> {
public:
    ECSEngine(unsigned int width, unsigned int height, const std::string& name);
    ~ECSEngine();

    // runs the game loop until the window closes or a system asks to exit
    void Run();

    SoundManager& GetSoundManager() override { return mSoundManager; }

    SpriteManager& GetSpriteManager() override { return mSpriteManager; }

    EntityManager<Components...>& GetEntityManager() override { return mEntityManager; }

    WindowManager& GetWindowManager() override { return mWindowManager; }

    SystemManager<Components...>& GetSystemManager() { return mSystemManager; }

private:
    // reads SFML events and updates every InputComponent; false means the window closed
    bool ProcessEvents();

    // draws collision boxes when debug mode is on (toggled with F1)
    void DebugDraw();

    EntityManager<Components...> mEntityManager;
    SystemManager<Components...> mSystemManager;
    SpriteManager mSpriteManager;
    SoundManager mSoundManager;
    WindowManager mWindowManager;

    // For debugging - pause and step the engine
    bool running, step;
    int debug;
};

// create the window and start with the game not running and debug drawing off
template <typename... Components>
ECSEngine<Components...>::ECSEngine(unsigned int width, unsigned int height,
                                    const std::string& name)
    : mWindowManager(width, height, name), running(false), step(false), debug(0) {}

template <typename... Components>
ECSEngine<Components...>::~ECSEngine() {}

template <typename... Components>
void ECSEngine<Components...>::Run() {
    auto window = mWindowManager.GetWindow();
    running = true;

    // one loop = one frame (capped at 60 fps by the WindowManager)
    while (running && window->isOpen()) {
        // handle keyboard / window events first, stop if the window was closed
        if (!ProcessEvents()) break;

        // clear last frame's drawing
        window->clear(sf::Color::Black);

        // run every system in order; stop if one of them returns kExit
        if (!mSystemManager.Run(this)) running = false;

        // draw debug boxes on top of the game, then show the finished frame
        DebugDraw();
        window->display();
    }
}

template <typename... Components>
bool ECSEngine<Components...>::ProcessEvents() {
    // save this frame's keys as "last frame" before reading new events,
    // so InputSystem can tell when a key was just pressed
    for (auto& entity : mEntityManager) {
        EntityID id = entity.GetID();
        if (mEntityManager.template HasComponent<InputComponent>(id)) {
            auto& input = mEntityManager.template GetComponent<InputComponent>(id);
            input.lastKeydown = input.keydown;
        }
    }

    // handle every event SFML has queued since last frame
    auto window = mWindowManager.GetWindow();
    while (const std::optional<sf::Event> event = window->pollEvent()) {
        // window closed -> stop the game
        if (event->is<sf::Event::Closed>()) {
            window->close();
            return false;
        }

        // work out which key changed and whether it went down or up
        // anything other than a key press / release is ignored
        bool isDown;
        sf::Keyboard::Scancode code;
        if (const auto* pressed = event->getIf<sf::Event::KeyPressed>()) {
            isDown = true;
            code = pressed->scancode;
        } else if (const auto* released = event->getIf<sf::Event::KeyReleased>()) {
            isDown = false;
            code = released->scancode;
        } else {
            continue;
        }

        // unknown keys have no slot in the bitset
        if (code == sf::Keyboard::Scancode::Unknown) continue;

        // F1 turns debug drawing on / off (!debug flips 0 to 1 and 1 to 0)
        if (isDown && code == sf::Keyboard::Scancode::F1) debug = !debug;

        // record the key state in every entity that takes input
        // the scancode is used as the index into the keydown bitset
        for (auto& entity : mEntityManager) {
            EntityID id = entity.GetID();
            if (mEntityManager.template HasComponent<InputComponent>(id)) {
                mEntityManager.template GetComponent<InputComponent>(id)
                    .keydown[static_cast<std::size_t>(code)] = isDown;
            }
        }
    }
    return true;
}

template <typename... Components>
void ECSEngine<Components...>::DebugDraw() {
    // debug drawing is off
    if (debug <= 0) return;

    // draw a red outline around every collision box
    for (auto& entity : mEntityManager) {
        EntityID id = entity.GetID();
        if (mEntityManager.template HasComponent<CollisionComponent>(id) &&
            mEntityManager.template HasComponent<LocationComponent>(id)) {
            // collisionBox is relative to the entity, so add the location for world coordinates
            Rect box = mEntityManager.template GetComponent<CollisionComponent>(id).collisionBox;
            const auto& loc = mEntityManager.template GetComponent<LocationComponent>(id).loc;
            box.topLeft.x += loc.x;
            box.topLeft.y += loc.y;
            mWindowManager.DebugFrameRect(box, 255, 0, 0);
        }
    }
}

}  // namespace ECSEngine
