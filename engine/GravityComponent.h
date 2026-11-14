#pragma once

#include "MathUtil.h"

namespace ECSEngine {

struct GravityComponent {
    GravityComponent(Point2D acceleration, int disabledTimer = 0)
        : acceleration(acceleration), disabledTimer(disabledTimer) {}

    Point2D acceleration;  // Acceleration in pixels per second squared
    int disabledTimer;
};

}  // namespace ECSEngine