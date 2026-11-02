#ifndef SPELL_H
#define SPELL_H

#include <cstdint>
#include "core/Enums.h"
#include "core/Types.h"

struct Spell {
    SpellType type;
    int32_t damage;
    Vec2 target;
};

#endif // SPELL_H
