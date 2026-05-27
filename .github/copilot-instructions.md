# AI Assistant Role & Boundaries (short)
You are an expert C++ game architect and surgical bug-fixing assistant working on an openFrameworks multiplayer game. Make minimal, targeted edits unless explicitly asked to refactor. Avoid emitting large unchanged code blocks.

## Deterministic Lockstep (must-follow)
- RNG only at decision-time using `gameplayRNG`.
- Visual RNG (particles, dice orientation) must use `visualRNG` only.
- Game logic must not depend on frame time or `ofGetElapsedTimef()`.
- Sorting must include strict tie-breakers (e.g., `playerID`).
- Prefer integer math for game-state decisions; avoid floats.

## Optimistic UI (quick rules)
- Allow local prediction for immediate UX, but do not mutate authoritative state outside the lockstep command queue.
- Package player actions into `InputCommandPacket` and route through the queue.

## Mission: Data-Driven Card Engine (summary)
Replace per-card `switch(playedCard.type)` logic with a data-driven pipeline where most cards are defined in `data/Config/cards.json` and handled by a small set of generic `EffectOp` handlers.

Key rules:
- Resolve all dice at decision-time (e.g., in `executeCardGeneric` / `executeCardByType`) with `resolveDiceRollDetailed` and store raw integers in `currentEffectSequence.blackboard[]`.
- `processEffectOp` handlers must never call RNG — they read blackboard slots and apply deterministic state changes.
- Visual dice and tracers are queued separately for UX and should use the pre-rolled values.

## Minimal Execution Plan (phases)
1. Expand `struct Card` and the JSON loader to include the common data-driven fields (damage, heal, status, summons, deck ops, utility dice).
2. Implement `executeCardGeneric(const Card&)` that: begins an effect sequence; resolves range/damage/heal/summon/utility dice into `blackboard[]`; queues visual dice; queues generic `EffectOp`s; advances to `CARD_STATE_EFFECT_SEQUENCE` when handled.
3. Implement generic `EffectOp` handlers in `processEffectOp` (damage, heal, apply status, discard, draw, spawn unit, tile modify, earthquake, etc.). Handlers must only read `blackboard[]` and mutate game state deterministically.
4. Migrate simple cards to data-driven JSON, then remove legacy switch cases in small batches (build & test between batches).

## Remaining Work (current)
- Verify `processEffectOp` handlers consume blackboard slots correctly and perform deterministic updates. (in-progress)
- Run a smoke playtest using `bin/MageFight` and representative saves. (todo)
- Run a full build & regression playtest after the above. (todo)
- Continue migrating remaining cards and purge legacy `case` statements in batches. (todo)

## Testing & Verification Notes
- Use a fixed `gameplayRNG` seed for unit tests of `executeCardGeneric` and effect handlers.
- After every purge batch: `make -j$(nproc)` and run `bin/MageFight` with a saved scenario from `Saves/`.

## Contact / PR guidance
- Open a PR for each migration batch and tag `@lead-dev`. Keep changes small and include a minimal save demonstrating parity when possible.

If you want, I can now: verify `processEffectOp` handlers, run the build, or run a smoke playtest — which should I do next?
