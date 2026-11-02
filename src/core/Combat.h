#pragma once

#include "Cards.h"
#include <algorithm>
#include <cstdint>

struct CombatUnit {
	int32_t health { 15 };
	int32_t maxHealth { 15 };

	// 5 Mitigation Layers
	int32_t block { 0 };
	int32_t fortification { 0 };
	int32_t barrier { 0 };
	int32_t holyBlock { 0 };
	int32_t ward { 0 };

	// Status Flags
	bool inGhostForm { false };
	bool isHellhound { false };
	bool isDemon { false };
	bool isSkeleton { false };
	bool hasVampireBite { false };
	bool hasWolfCall { false };
};

struct MitigationResult {
	int32_t initialDamage { 0 };
	int32_t damageBlocked { 0 };
	int32_t damageFortified { 0 };
	int32_t damageBarriered { 0 };
	int32_t damageHolyBlocked { 0 };
	int32_t damageWarded { 0 };
	int32_t finalDamageTaken { 0 };
};

// Port of ofApp::applyDamageWithMitigations
inline MitigationResult apply_damage_with_mitigations(CombatUnit & target, int32_t baseDamage, DamageType type) {
	MitigationResult res {};
	int32_t dmg = baseDamage;

	// 1. Ghost Form Immunities
	if (target.inGhostForm && (type == DamageType::PHYSICAL || type == DamageType::PIERCING)) {
		res.initialDamage = 0;
		res.finalDamageTaken = 0;
		return res;
	}

	// 2. Vulnerabilities (Double damage multipliers)
	bool holyDoubled = false;
	if (target.inGhostForm && type == DamageType::HOLY) {
		dmg *= 2;
		holyDoubled = true;
	}

	if (!holyDoubled && type == DamageType::HOLY) {
		if (target.isHellhound || target.isDemon || target.isSkeleton || target.hasVampireBite) {
			dmg *= 2;
		}
	}

	if (type == DamageType::PIERCING && target.hasWolfCall) {
		dmg *= 2;
	}

	res.initialDamage = dmg;
	int32_t remaining = dmg;

	auto absorb = [](int32_t & shield, int32_t & rem) -> int32_t {
		int32_t taken = std::min(shield, rem);
		shield -= taken;
		rem -= taken;
		return taken;
	};

	// 3. Absorption Priority by Damage Type (Exact logic from ofApp.cpp lines 1180-1205)
	switch (type) {
	case DamageType::HOLY:
		res.damageHolyBlocked = absorb(target.holyBlock, remaining);
		res.damageBarriered = absorb(target.barrier, remaining);
		res.damageWarded = absorb(target.ward, remaining);
		break;

	case DamageType::PHYSICAL:
		res.damageBlocked = absorb(target.block, remaining);
		res.damageFortified = absorb(target.fortification, remaining);
		res.damageWarded = absorb(target.ward, remaining);
		break;

	case DamageType::PIERCING:
		res.damageFortified = absorb(target.fortification, remaining);
		res.damageBarriered = absorb(target.barrier, remaining);
		res.damageWarded = absorb(target.ward, remaining);
		break;

	default: // MAGIC, ELECTRIC, FIRE, POISON
		res.damageBarriered = absorb(target.barrier, remaining);
		res.damageWarded = absorb(target.ward, remaining);
		break;
	}

	res.finalDamageTaken = remaining;
	target.health = std::max(0, target.health - remaining);
	return res;
}