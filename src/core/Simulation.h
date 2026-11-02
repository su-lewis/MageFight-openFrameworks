#ifndef SIMULATION_H
#define SIMULATION_H

#include "GameState.h"
#include "PlayerState.h"
#include "Command.h"
#include <vector>

inline void simulate_tick(GameState& state, PlayerState& player, const std::vector<Command>& commands, uint32_t currentTick) {
    for (const auto& command : commands) {
        if (command.tick == currentTick) {
            switch (command.type) {
                case CommandType::MOVE:
                    player.position = command.target;
                    break;
                case CommandType::CAST_SPELL:
                    if (command.spellType == SpellType::FIREBALL) {
                        player.health -= 10;
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

#endif // SIMULATION_H
