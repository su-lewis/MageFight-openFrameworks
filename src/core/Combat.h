#ifndef COMBAT_H
#define COMBAT_H

#include <cstdint>
#include "Cards.h"

struct CombatUnit {
    int32_t health{15};
    int32_t maxHealth{15};
    int32_t block{0};
    int32_t fortification{0};
    int32_t barrier{0};
    int32_t holyBlock{0};
    int32_t ward{0};
    bool inGhostForm{false};
    bool isHellhound{false};
    bool isDemon{false};
    bool isSkeleton{false};
    bool hasVampireBite{false};
    bool hasWolfCall{false};
};

struct MitigationResult {
    int32_t initialDamage;
    int32_t holyBlockAbsorbed;
    int32_t barrierAbsorbed;
    int32_t wardAbsorbed;
    int32_t finalDamageTaken;
};

MitigationResult apply_damage_with_mitigations(CombatUnit & target, int32_t baseDamage, DamageType type) {
    MitigationResult result;
    result.initialDamage = baseDamage;

    if (target.inGhostForm && (type == DamageType::PHYSICAL || type == DamageType::PIERCING)) {
        result.finalDamageTaken = 0;
        return result;
    }

    if (type == DamageType::HOLY && (target.inGhostForm || target.isHellhound || target.isDemon || target.isSkeleton || target.hasVampireBite)) {
        baseDamage *= 2;
    }

    if (type == DamageType::PIERCING && target.hasWolfCall) {
        baseDamage *= 2;
    }

    int32_t damageToAbsorb = baseDamage;

    switch (type) {
        case DamageType::HOLY:
            damageToAbsorb -= target.holyBlock;
            target.holyBlock = 0;
            if (damageToAbsorb > 0) {
                damageToAbsorb -= target.barrier;
                target.barrier = 0;
            }
            if (damageToAbsorb > 0) {
                damageToAbsorb -= target.ward;
                target.ward = 0;
            }
            break;
        case DamageType::PHYSICAL:
            damageToAbsorb -= target.block;
            target.block = 0;
            if (damageToAbsorb > 0) {
                damageToAbsorb -= target.fortification;
                target.fortification = 0;
            }
            if (damageToAbsorb > 0) {
                damageToAbsorb -= target.ward;
                target.ward = 0;
            }
            break;
        case DamageType::PIERCING:
            damageToAbsorb -= target.fortification;
            target.fortification = 0;
            if (damageToAbsorb > 0) {
                damageToAbsorb -= target.barrier;
                target.barrier = 0;
            }
            if (damageToAbsorb > 0) {
                damageToAbsorb -= target.ward;
                target.ward = 0;
            }
            break;
        default:
            damageToAbsorb -= target.barrier;
            target.barrier = 0;
            if (damageToAbsorb > 0) {
                damageToAbsorb -= target.ward;
                target.ward = 0;
            }
            break;
    }

    result.finalDamageTaken = std::max(0, damageToAbsorb);
    target.health = std::max(0, target.health - result.finalDamageTaken);

    return result;
}

#endif // COMBAT_H
