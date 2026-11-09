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

template <typename... Components>
class EngineView {
public:
    virtual SoundManager& GetSoundManager() = 0;
    virtual SpriteManager& GetSpriteManager() = 0;
    virtual EntityManager<Components...>& GetEntityManager() = 0;
    virtual WindowManager& GetWindowManager() = 0;
};

template <typename... Components>
class ECSEngine : public EngineView<Components...> {
public:
    ECSEngine(unsigned int width, unsigned int height, const std::string& name);
    ~ECSEngine();

    void Run();

    SoundManager& GetSoundManager() override { return mSoundManager; }

    SpriteManager& GetSpriteManager() override { return mSpriteManager; }

    EntityManager<Components...>& GetEntityManager() override { return mEntityManager; }

    WindowManager& GetWindowManager() override { return mWindowManager; }

    SystemManager<Components...>& GetSystemManager() { return mSystemManager; }

private:
    bool ProcessEvents();
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
    while (running && window->isOpen()) {
        if (!ProcessEvents()) break;
        window->clear(sf::Color::Black);
        if (!mSystemManager.Run(this)) running = false;
        DebugDraw();
        window->display();
    }
}

template <typename... Components>
bool ECSEngine<Components...>::ProcessEvents() {
    for (auto& entity : mEntityManager) {
        EntityID id = entity.GetID();
        if (mEntityManager.template HasComponent<InputComponent>(id)) {
            auto& input = mEntityManager.template GetComponent<InputComponent>(id);
            input.lastKeydown = input.keydown;
        }
    }
    auto window = mWindowManager.GetWindow();
    while (const std::optional<sf::Event> event = window->pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window->close();
            return false;
        }

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
        if (code == sf::Keyboard::Scancode::Unknown) continue;

        if (isDown && code == sf::Keyboard::Scancode::F1) debug = !debug;

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
    if (debug <= 0) return;
    for (auto& entity : mEntityManager) {
        EntityID id = entity.GetID();
        if (mEntityManager.template HasComponent<CollisionComponent>(id) &&
            mEntityManager.template HasComponent<LocationComponent>(id)) {
            Rect box = mEntityManager.template GetComponent<CollisionComponent>(id).collisionBox;
            const auto& loc = mEntityManager.template GetComponent<LocationComponent>(id).loc;
            box.topLeft.x += loc.x;
            box.topLeft.y += loc.y;
            mWindowManager.DebugFrameRect(box, 255, 0, 0);
        }
    }
}

}  // namespace ECSEngine
