#pragma once

#include "MathUtil.h"

namespace ECSEngine {

struct CameraShake {
    int frameLimit;
    Point2D offset;
};

}  // namespace ECSEngine