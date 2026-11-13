#pragma once

#include <bitset>
#include <unordered_map>

#include "MathUtil.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Window/Keyboard.hpp>

namespace ECSEngine {

// keyboard state and movement state for an entity controlled by the player
// ECSEngine::ProcessEvents writes keydown / lastKeydown, InputSystem reads them every frame
struct InputComponent {
    // lastDash starts at 0, so the first dash is only allowed once kDashInterval
    // frames (1 second) have passed since the game started
    InputComponent(EntityID whichEntity) : entity(whichEntity), lastDash(0) {}

    // the entity this input belongs to
    EntityID entity;

    // frame number of the most recent dash, used for the dash cooldown
    int lastDash;

    // frames left in the current dash (0 means not dashing)
    int dashFramesLeft = 0;

    // frames left where left/right input is ignored after a wall jump
    // keeps the player moving away from the wall instead of steering straight back
    int wallJumpLock = 0;

    // side of the last wall touched: -1 = wall on our left, +1 = wall on our right, 0 = none
    int wallSide = 0;

    // frames left where the player still counts as "on the wall" after losing contact
    // lets a jump still work just after letting go of the arrow key or sliding off a wall
    int wallGraceFrames = 0;

    // same-wall hops left before the player must touch the ground or a different wall
    int wallHopsLeft = 0;

    // one bit per keyboard scancode, true while that key is held down
    std::bitset<sf::Keyboard::ScancodeCount> keydown;

    // keydown from the previous frame, used to detect the frame a key was first pressed
    std::bitset<sf::Keyboard::ScancodeCount> lastKeydown;
};

}  // namespace ECSEngine
