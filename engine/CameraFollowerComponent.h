#pragma once

#include "Entity.h"

namespace ECSEngine {

struct CameraFollower {
    EntityID entityID;  // The entity that this camera follows
};

}  // namespace ECSEngine