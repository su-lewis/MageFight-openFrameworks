#ifndef BUFF_H
#define BUFF_H

#include <cstdint>
#include "core/Enums.h"
#include "core/Types.h"

struct Buff {
    BuffType type;
    uint32_t duration;
    Vec2 target;
};

#endif // BUFF_H
