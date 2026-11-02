#ifndef SIMULATION_H
#define SIMULATION_H

#include "GameState.h"
#include "UnitState.h"
#include "Command.h"
#include "Combat.h"
#include <vector>

inline void simulate_tick(GameState& state, UnitState& unit, const std::vector<Command>& commands, uint32_t currentTick) {
    for (const auto& command : commands) {
        if (command.tick == currentTick) {
            switch (command.type) {
                case CommandType::MOVE_UNIT:
                    unit.x = command.params[0];
                    unit.y = command.params[1];
                    break;
                default:
                    break;
            }
        }
    }
}

#endif // SIMULATION_H

