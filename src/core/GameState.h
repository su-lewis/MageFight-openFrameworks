#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <cstdint>
#include "core/Enums.h"
#include "core/Types.h"

struct GameState {
    Vec3 cameraPosition;
    Rect gameBounds;
    bool isPaused;
};

#endif // GAMESTATE_H
