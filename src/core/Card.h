#ifndef CARD_H
#define CARD_H

#include <cstdint>
#include "core/Enums.h"
#include "core/Types.h"

struct Card {
    CardType type;
    uint32_t cost;
    Vec2 position;
};

#endif // CARD_H
