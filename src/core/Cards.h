#ifndef CARDS_H
#define CARDS_H

#include <cstdint>
#include <string>

enum class DamageType : uint8_t {
    PHYSICAL = 0,
    PIERCING,
    MAGIC,
    ELECTRIC,
    FIRE,
    HOLY,
    POISON
};

enum class CardType : uint8_t {
    CARD_NONE = 0,
    CARD_PUNCH,
    CARD_KICK,
    CARD_BASH,
    CARD_STAB,
    CARD_SLASH,
    CARD_HEAL,
    CARD_LESSER_HEAL,
    CARD_FIREBALL,
    CARD_SHOCK,
    CARD_BURST_OF_LIGHT,
    CARD_DISPEL,
    CARD_AMNESIA,
    CARD_DOUBLE_HANDED,
    CARD_WISDOM_BOON,
    CARD_BLOCKING_BOON,
    CARD_CONSTITUTION_BOON,
    CARD_TELEPORT,
    CARD_MAGIC_BLAST,
    CARD_TRAIN,
    CARD_STUDY
};

struct Card {
    int32_t id{0};
    std::string name;
    CardType type{CardType::CARD_NONE};
    int32_t cost{0};
    int32_t baseDamage{0};
    int32_t damageDiceNum{0};
    int32_t damageDiceSides{0};
    DamageType damageType{DamageType::PHYSICAL};
    int32_t blockAmount{0};
    int32_t fortificationAmount{0};
    int32_t barrierAmount{0};
    int32_t holyBlockAmount{0};
    int32_t wardAmount{0};
    int32_t healAmount{0};
};

#endif // CARDS_H
